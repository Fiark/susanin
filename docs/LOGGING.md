# Logging and monitoring

## Live decision stream

```routeros
/log print follow-only where message~"AUTO-AWG:"
```

## Controller status

```routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin status" no-sh timeout=60
```

## Structural drift

```routeros
/container/shell susanin-controller cmd="/usr/local/bin/susanin apply --dry-run" no-sh timeout=60
```

## Learned destinations

```routeros
/ip firewall address-list print where list~"auto_awg_"
```

## Health transitions only

```routeros
/log print where message~"AUTO-AWG: tunnel"
```

## Controller bootstrap

```routeros
/log print where message~"SUSANIN:"
```

Remember that RouterOS logs can include destination IP addresses and ports. Avoid uploading raw persistent logs publicly without review.
