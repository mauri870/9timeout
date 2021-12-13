# 9timeout

> Just like timeout(1).

```
usage: timeout [duration [command ...]]
```

```sh
#!/bin/rc

if (! timeout 2 sleep 5)
	echo "Timed out :("
```
