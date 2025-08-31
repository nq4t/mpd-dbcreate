# Provide empty systemd_dep for build compatibility
# mpd-dbcreate doesn't need systemd as it's not a daemon
systemd_dep = dependency('', required: false)
conf.set('ENABLE_SYSTEMD_DAEMON', false)