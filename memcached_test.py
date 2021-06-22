import subprocess

def memaslap_test(qemu_cores, qemu_mem, memcached_mem):
    logfile = open("memaslap.log", "w")
    qemu_cmd = "qemu-system-x86_64 -enable-kvm " +\
        "-kernel output/images/bzImage " +\
        "-initrd output/images/rootfs.cpio.gz " +\
        "-append \"console=ttyS0\" " +\
        "-cpu host " +\
        "-smp "+ str(qemu_cores) + " " +\
        "-m "+ str(qemu_mem) + " " +\
        "-usb " +\
        "-chardev pty,id=ser0 " +\
        "-serial chardev:ser0 " +\
        "-net user,hostfwd=tcp::10022-:22,hostfwd=tcp::11212-:11211 -net nic " +\
        "-nographic " +\
        "-drive file=swap-1g.raw,format=raw,if=ide"

    print("starting QEMU")
    qemu_instance = subprocess.Popen(qemu_cmd, stdout=subprocess.PIPE, shell=True)
    try:
        qemu_out, qemu_err = qemu_instance.communicate(timeout = 5)
        raise SystemError("QEMU closed unexpectedly with STDOUT:\n" + str(qemu_out) + "\nSTDERR:\n" + str(qemu_err))
    except subprocess.TimeoutExpired:
        pass

    print("sshing into QEMU")
    ssh_cmd = "sshpass -p \"root\" " +\
        "ssh root@localhost " +\
        "-p 10022 " +\
        "-o \"UserKnownHostsFile /dev/null\" " +\
        "-o StrictHostKeyChecking=no " +\
        "\"exec memcached -u nobody -m " + str(memcached_mem) + "\""

    ssh = subprocess.Popen(ssh_cmd, stdout=subprocess.PIPE, shell=True)

    memaslap_cmd = "memaslap -s localhost:11212 –facebook –division=50"
    print("first memaslap run")
    memaslap = subprocess.Popen(memaslap_cmd, stdout=subprocess.PIPE, shell=True)

    try:
        ms_out, ms_err = memaslap.communicate(timeout = 10*60 + 5)
    except subprocess.TimeoutExpired:
        raise SystemError("Memaslap didn't close after 10m")

    print("second memaslap run")
    memaslap = subprocess.Popen(memaslap_cmd, stdout=logfile, shell=True)
    try:
        ms_out, ms_err = memaslap.communicate(timeout = 10*60 + 5)
    except subprocess.TimeoutExpired:
        raise SystemError("Memaslap didn't close after 10m")

    qemu_instance.kill()
    ssh.kill()
    logfile.close()


memaslap_test(4, 1600, 2000)
