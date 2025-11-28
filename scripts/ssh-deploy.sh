
#!/bin/bash
# Script to deploy Cyberia Client via SSH

cd /home/dd/cyberia-client
git reset
underpost cmt . cd ssh-cyberia-client --empty
underpost push . underpostnet/cyberia-client
