$SCRIPT="memcached -u nobody -m " "$MEMCACHED_MEM"

sshpass -p "root" \ # we don't use a more secure option because the password is "root"
  ssh root@localhost \
  -p 10022 \
  -o "UserKnownHostsFile /dev/null" \
  -o StrictHostKeyChecking=no \
  "bash -s" < memcached_on_server.sh
