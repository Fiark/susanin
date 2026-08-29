# Susanin v0.11.3 credentialless bootstrap
# Requires the architecture-matching container image uploaded as susanin.tar.
# No username/password input and no environment list are required.
# Controller network: 172.31.254.0/30 (RouterOS .1, Susanin .2).
#
# RouterOS 7.23.3 testing showed that file secret writes work reliably from a
# normal /system script but may produce a zero-byte file when performed directly
# inside /import. Therefore /import only installs the bootstrap worker. The worker
# performs secret rotation, exact read-back verification, agent synchronization,
# mount creation, image extraction and first start in normal script context.

# Reject foreign use of the controller subnet, but tolerate an interrupted
# previous Susanin bootstrap on bridge-susanin.
:foreach susaninAddressId in=[/ip address find where network=172.31.254.0] do={
    :local susaninAddressInterface [/ip address get $susaninAddressId interface]
    :if ($susaninAddressInterface != "bridge-susanin") do={
        :error ("Susanin bootstrap: 172.31.254.0/30 is already used by " . $susaninAddressInterface)
    }
}

# Standard RouterOS container topology: VETH -> isolated bridge -> RouterOS IP.
:if ([:len [/interface veth find where name="veth-susanin"]] = 0) do={
    /interface veth add name=veth-susanin address=172.31.254.2/30 gateway=172.31.254.1 comment="SUSANIN: controller veth"
}
:if ([:len [/interface bridge find where name="bridge-susanin"]] = 0) do={
    /interface bridge add name=bridge-susanin protocol-mode=none comment="SUSANIN: isolated controller bridge"
}
:if ([:len [/interface bridge port find where bridge="bridge-susanin" and interface="veth-susanin"]] = 0) do={
    /interface bridge port add bridge=bridge-susanin interface=veth-susanin
}
:if ([:len [/ip address find where interface="bridge-susanin" and address="172.31.254.1/30"]] = 0) do={
    /ip address add address=172.31.254.1/30 interface=bridge-susanin comment="SUSANIN: controller gateway"
}

# Internal machine group. The restricted agent account itself is provisioned by
# the detached worker only after the machine secret has been verified.
:if ([:len [/user group find where name="susanin-agent"]] = 0) do={
    /user group add name="susanin-agent" policy=read,write,test,api
} else={
    /user group set [find where name="susanin-agent"] policy=read,write,test,api
}

# Restrict the RouterOS API to existing trusted addresses plus the isolated
# Susanin controller address. Always enable the service explicitly: RouterOS
# 7.23.3 can return the disabled property in a form that is unsafe to branch on
# from an imported script, while /ip service enable is idempotent.
:if ([:len [/ip service find where name="api"]] = 0) do={
    :error "Susanin bootstrap: RouterOS API service not found"
}
/ip service enable [find where name="api"]
:local susaninApiAddresses [:tostr [/ip service get [find where name="api"] address]]
:if ([:len $susaninApiAddresses] = 0) do={
    /ip service set [find where name="api"] address=172.31.254.2/32
} else={
    :if ([:find $susaninApiAddresses "172.31.254.2"] = nil) do={
        /ip service set [find where name="api"] address=($susaninApiAddresses . ",172.31.254.2/32")
    }
}
:if ([:len [/ip firewall filter find where comment="SUSANIN: allow controller API"]] = 0) do={
    /ip firewall filter add chain=input action=accept protocol=tcp src-address=172.31.254.2 dst-address=172.31.254.1 dst-port=8728 place-before=0 comment="SUSANIN: allow controller API"
}

# Remove obsolete bootstrap helpers from earlier previews.
/system scheduler remove [find where name="susanin-bootstrap-start"]
/system script remove [find where name="susanin-bootstrap-start"]
/system scheduler remove [find where name="susanin-bootstrap-worker"]
/system script remove [find where name="susanin-bootstrap-worker"]
/system scheduler remove [find where name="susanin-bootstrap-cleanup"]

# The worker is intentionally a normal RouterOS script. It is a small state
# machine: replace an older controller, provision+verify the secret, add the new
# container, then wait on later scheduler ticks for extraction to complete and
# start it. No secret is ever placed in environment variables or command args.
/system script add name="susanin-bootstrap-worker" policy=read,write,test,ftp,policy,password dont-require-permissions=yes source={
    # If our target-version container already exists, wait for extraction and
    # start it. The worker then disables its own scheduler; successful setup
    # removes both helper objects permanently.
    :if ([:len [/container find where name="susanin-controller"]] > 0) do={
        # During asynchronous extraction RouterOS leaves tag/os/arch empty, but
        # root-dir is already populated. Identify our in-progress target by its
        # versioned root-dir, otherwise the worker would mistake it for an old
        # container and continuously delete/re-add it.
        :local susaninBootstrapRoot [:tostr [/container get [find where name="susanin-controller"] root-dir]]
        :if ($susaninBootstrapRoot = "/susanin-controller-v0113") do={
            :local susaninBootstrapArch [:tostr [/container get [find where name="susanin-controller"] arch]]
            :if ($susaninBootstrapArch = "") do={ :return }
            :local susaninBootstrapTag [:tostr [/container get [find where name="susanin-controller"] tag]]
            :if ($susaninBootstrapTag != "0.11.3") do={
                /system scheduler disable [find where name="susanin-bootstrap-worker"]
                :log error ("SUSANIN: extracted image tag mismatch, expected 0.11.3 got " . $susaninBootstrapTag)
                :return
            }

            :local susaninStartOK true
            :onerror susaninStartError in={
                /container start [find where name="susanin-controller"]
            } do={
                :set susaninStartOK false
                :log warning ("SUSANIN: bootstrap start returned error; will retry: " . $susaninStartError)
            }
            :if ($susaninStartOK = false) do={ :return }

            # The bootstrap worker carries temporary elevated policies. Do not
            # leave it behind for the restricted susanin-agent to clean up later:
            # schedule an admin-owned one-shot self-cleaner immediately.
            /system scheduler remove [find where name="susanin-bootstrap-cleanup"]
            /system scheduler add name="susanin-bootstrap-cleanup" interval=2s policy=read,write,policy,test,password comment="SUSANIN: one-shot bootstrap helper cleanup" on-event=":delay 1s; /system scheduler remove [find where name=\"susanin-bootstrap-worker\"]; /system script remove [find where name=\"susanin-bootstrap-worker\"]; /system scheduler remove [find where name=\"susanin-bootstrap-cleanup\"]"
            /system scheduler disable [find where name="susanin-bootstrap-worker"]
            :log info "SUSANIN: controller bootstrap finished; helper cleanup queued"
            :return
        }

        # A different root-dir belongs to an older Susanin controller. It is
        # safe to replace because the RouterOS adaptive routing data-plane is
        # independent from the control-plane container.
        :onerror susaninStopError in={ /container stop [find where name="susanin-controller"] } do={}
        :delay 1s
        :onerror susaninRemoveError in={ /container remove [find where name="susanin-controller"] } do={}
        :delay 1s
        :if ([:len [/container find where name="susanin-controller"]] > 0) do={ :return }
    }

    # Detach old consumers before rotating the machine credential.
    /container mounts remove [find where list="susanin-secret"]
    /container mounts remove [find where list="susanin-data"]

    :if ([:len [/file find where name="susanin-secrets"]] = 0) do={
        /file add name=susanin-secrets type=directory
    }
    :if ([:len [/file find where name="susanin-secrets/routeros_password"]] = 0) do={
        /file add name=susanin-secrets/routeros_password type=file
    }
    :if ([:len [/file find where name="susanin-data"]] = 0) do={
        /file add name=susanin-data type=directory
    }

    :local susaninGeneratedPassword [:rndstr length=48]
    /file set [find where name="susanin-secrets/routeros_password"] contents=$susaninGeneratedPassword
    :local susaninWrittenSize [/file get [find where name="susanin-secrets/routeros_password"] size]
    :local susaninWrittenPassword [/file get [find where name="susanin-secrets/routeros_password"] contents as-string]
    :if (($susaninWrittenSize != 48) || ([:len $susaninWrittenPassword] != 48) || ($susaninWrittenPassword != $susaninGeneratedPassword)) do={
        :log error "SUSANIN: secret verification failed; agent password was NOT changed"
        :return
    }

    # Synchronize the restricted RouterOS machine identity only after successful
    # exact read-back of the mounted secret.
    :if ([:len [/user find where name="susanin-agent"]] = 0) do={
        /user add name="susanin-agent" group="susanin-agent" address=172.31.254.2/32 password=$susaninGeneratedPassword
    } else={
        /user set [find where name="susanin-agent"] group="susanin-agent" address=172.31.254.2/32 password=$susaninGeneratedPassword
    }

    /container mounts add list=susanin-secret src=susanin-secrets dst=/run/secrets
    /container mounts add list=susanin-data src=susanin-data dst=/data

    :onerror susaninContainerAddError in={
        /container add file=susanin.tar interface=veth-susanin root-dir=/susanin-controller-v0113 mountlists=susanin-secret,susanin-data name=susanin-controller logging=yes start-on-boot=yes cmd=daemon
    } do={
        :log error ("SUSANIN: container add failed: " . $susaninContainerAddError)
        :return
    }
    :log info "SUSANIN: container queued for extraction"
}

# A repeating scheduler launches the worker outside /import. Official RouterOS
# scheduler behavior executes an interval task repeatedly; the worker disables
# this scheduler after the target container is ready and first-started.
/system scheduler add name="susanin-bootstrap-worker" interval=2s on-event="susanin-bootstrap-worker" policy=read,write,test,ftp,policy,password comment="SUSANIN: temporary credentialless bootstrap worker"

:put ""
:put "Susanin bootstrap worker installed. No credentials were requested."
:put "The worker will rotate/verify the internal secret and start the controller automatically."
:put "When /container print shows susanin-controller as RUNNING, run:"
:put "/container/shell susanin-controller cmd=\"/usr/local/bin/susanin setup\" no-sh timeout=300"
