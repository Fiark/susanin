# Susanin v0.11.3 controller-only cleanup.
# Keeps the RouterOS adaptive routing data-plane intact.

/system scheduler remove [find where name~"susanin-bootstrap-"]
/system script remove [find where name~"susanin-bootstrap-"]

:if ([:len [/container find where name="susanin-controller"]] > 0) do={
    :onerror e in={ /container stop [find where name="susanin-controller"] } do={}
    :delay 1s
    :onerror e in={ /container remove [find where name="susanin-controller"] } do={}
}
/container mounts remove [find where list="susanin-secret"]
/container mounts remove [find where list="susanin-data"]
/ip firewall filter remove [find where comment="SUSANIN: allow controller API"]
/ip address remove [find where comment="SUSANIN: controller gateway"]
/interface bridge port remove [find where bridge="bridge-susanin" and interface="veth-susanin"]
/interface veth remove [find where name="veth-susanin"]
/interface bridge remove [find where name="bridge-susanin"]
/user remove [find where name="susanin-agent"]
/user group remove [find where name="susanin-agent"]

:if ([:len [/ip service find where name="api"]] > 0) do={
    :local a [:tostr [/ip service get [find where name="api"] address]]
    :if ($a = "172.31.254.2/32") do={
        /ip service disable [find where name="api"]
        /ip service set [find where name="api"] address=""
    } else={
        :if ([:find $a "172.31.254.2"] != nil) do={
            :put "NOTE: API allowed-address contains Susanin address together with other trusted sources; review /ip service manually."
        }
    }
}

:put "Susanin controller removed. RouterOS adaptive routing data-plane was left running."
