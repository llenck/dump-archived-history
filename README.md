dump-archived-history
---------------------

If you want to keep your bash history forever, but also don't want bash taking up 10M of RAM due to 500K lines of history, the obvious solution is to use something like `logrotate(1)` to regularly clean up your history. However, you might also want to quickly view past history files; and that is where this program comes in.

This could of course also be implemented in a bash script, but in that case, respecting `HISTTIMEFORMAT` would probably be too slow; this program should be fast enough in most cases.
