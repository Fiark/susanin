# Susanin v0.11.4-rc1 full uninstall.
# Removes Susanin controller + Susanin-managed adaptive routing data-plane.
# Does NOT remove the selected VPN/tunnel itself.

:put "=== SUSANIN FULL UNINSTALL ==="

# Stop managed schedules first.
/system scheduler remove [find where name="auto-awg-health"]
/system scheduler remove [find where name="auto-awg-fast"]
/system scheduler remove [find where name="auto-awg-detect"]
/system scheduler remove [find where name="auto-awg-judge"]

# Remove managed and safety mangle rules.
/ip firewall mangle remove [find where comment~"^AUTO-AWG:"]
/ip firewall mangle remove [find where comment~"^SUSANIN: safety bypass"]

# Remove NAT only when Susanin created it.
/ip firewall nat remove [find where comment="SUSANIN: masquerade selected tunnel"]

# Remove temporary learned state.
/ip firewall address-list remove [find where list="auto_awg_watch_tcp"]
/ip firewall address-list remove [find where list="auto_awg_test_tcp"]
/ip firewall address-list remove [find where list="auto_awg_ok_tcp"]
/ip firewall address-list remove [find where list="auto_awg_cooldown_tcp"]
/ip firewall address-list remove [find where list="auto_awg_watch_udp"]
/ip firewall address-list remove [find where list="auto_awg_test_udp"]
/ip firewall address-list remove [find where list="auto_awg_ok_udp"]
/ip firewall address-list remove [find where list="auto_awg_cooldown_udp"]
/ip firewall address-list remove [find where list="auto_awg_health_fail"]

# Remove current managed connections if present.
/ip firewall connection remove [find where connection-mark="auto-awg-test-conn"]
/ip firewall connection remove [find where connection-mark="auto-awg-ok-conn"]

# Managed scripts + upgrade safety copies/temp objects.
/system script remove [find where name="auto-awg-health"]
/system script remove [find where name="auto-awg-fast"]
/system script remove [find where name="auto-awg-detect"]
/system script remove [find where name="auto-awg-judge"]
/system script remove [find where name~"susanin-stage-"]
/system script remove [find where name~"susanin-backup-"]
/system script remove [find where name~"susanin-validate-"]

# Remove a route only if Susanin itself created and marked it.
/ip route remove [find where comment="SUSANIN: default route via selected tunnel"]

# Bootstrap helpers.
/system scheduler remove [find where name~"susanin-bootstrap-"]
/system script remove [find where name~"susanin-bootstrap-"]

# Controller.
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

# Remove non-secret selection and machine secret when possible.
:onerror e in={ /file remove [find where name="susanin-data/susanin.conf"] } do={}
:onerror e in={ /file remove [find where name="susanin-secrets/routeros_password"] } do={}
:onerror e in={ /file remove [find where name="susanin-data"] } do={}
:onerror e in={ /file remove [find where name="susanin-secrets"] } do={}

# Restore API to disabled only when Susanin was its only allowed source.
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

:put "Susanin removed. The VPN/tunnel interface itself was preserved."
:put "If routing table 'susanin' is now empty and was created only for Susanin, remove it manually after review."
