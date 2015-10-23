#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

FILES_INSTALL_DIR=/lib/web/templates

FILES=                          \
	customer-defines.tem	\
	tms-cfg-form.tem        \
	tms-defines.tem         \
	tms-layout.tem          \
	tms-graph-utils.tem     \
	tms-utils.tem           \
	home.tem                \
	failure.tem             \
	help-index.tem          \
	logging.tem             \
	logging-layout.tem      \
	login.tem               \
	logout.tem              \
	logout-reboot.tem       \
	mon-cpu.tem             \
	mon-disks.tem           \
	mon-fsmnt.tem           \
	mon-mem.tem             \
	mon-network.tem         \
	mon-summary.tem         \
	mon-summary-common.tem  \
	setup-arp.tem           \
	setup-certificates.tem  \
	setup-configs.tem       \
	setup-configs-view.tem  \
	setup-date.tem          \
	setup-dns.tem           \
	setup-faults.tem        \
	setup-hostname.tem      \
	setup-hosts.tem         \
	setup-interface.tem     \
	setup-license.tem       \
	setup-locale.tem        \
	setup-logging.tem       \
	setup-myacct.tem        \
	setup-reboot.tem        \
	setup-reboot-confirm.tem\
	setup-routing.tem       \
	setup-ssh.tem           \
	setup-snmp.tem          \
	setup-snmp-user-edit.tem \
	setup-snmp-notif-sink-edit.tem \
	setup-upgrade.tem       \
	setup-upgrade-status.tem \
	setup-upgrade-status-common.tem \
	setup-users.tem         \
	setup-users-capabs.tem  \
	setup-users-acls.tem    \
	setup-users-add.tem     \
	setup-users-edit.tem    \
	setup-web.tem           \
	get-graph.tem           \
	get-graph-mult.tem      \
	get-mon-summary.tem     \
	get-log-lines.tem       \
	get-setup-upgrade-status.tem \
	get-setup-upgrade-complete.tem 	  \

ifneq (${PROD_FEATURE_AAA},0)
	FILES+=                            \
	mon-aaa-logins.tem      \
	setup-aaa.tem           \

endif

ifneq (${PROD_FEATURE_RADIUS},0)
	FILES+=                            \
	setup-radius.tem        \

endif

ifneq (${PROD_FEATURE_TACACS},0)
	FILES+=                            \
	setup-tacacs.tem        \

endif

ifneq (${PROD_FEATURE_LDAP},0)
	FILES+=                            \
	setup-ldap.tem          \

endif

ifneq (${PROD_FEATURE_IPV6},0)
	FILES+=                            \
	setup-neighbors.tem     \

endif

ifneq (${PROD_FEATURE_NTP_CLIENT},0)
	FILES+=                            \
	setup-ntp.tem           \

endif

ifneq (${PROD_FEATURE_CLUSTER},0)
	FILES+=                            \
	mon-cluster.tem                    \
	mon-cluster-versions.tem           \
	get-cluster-status.tem  	   \
	setup-cluster.tem	           \
	setup-cluster-node.tem             \
	setup-cluster-node-stop.tem        \
	setup-cluster-node-reboot.tem      \
	setup-cluster-upgrade.tem          \
	setup-cluster-upgrade-complete.tem \
	setup-cluster-upgrade-step.tem     \


endif

#	setup-stats.tem         \
#	setup-stats-edit.tem    \

ifneq (${PROD_FEATURE_FRONT_PANEL},0)
	FILES+=                 \
		setup-lcd.tem
endif

ifneq (${PROD_FEATURE_CMC_SERVER},0)
	FILES+=                      \
		mon-cmc.tem               \
		mon-cmc-detail.tem        \
		mon-cmc-common.tem        \
		tms-cmc-layout.tem        \
		setup-cmc-appliances.tem  \
		setup-cmc-edit-appl.tem   \
		setup-cmc-groups.tem      \
		setup-cmc-identities.tem  \
		setup-cmc-server.tem      \
		setup-cmc-status.tem      \
		setup-cmc-rendv.tem       \
		profile-cmc-main.tem 	  \
		profile-cmc-apply.tem 	  \
		profile-cmc-banners.tem   \
		profile-cmc-cmd.tem 	  \
		profile-cmc-aaa.tem 	  \
		profile-cmc-dns.tem 	  \
		profile-cmc-faults.tem 	  \
		profile-cmc-licensing.tem \
		profile-cmc-logging.tem   \
		profile-cmc-ldap.tem   	  \
		profile-cmc-ntp.tem    	  \
		profile-cmc-radius.tem    \
		profile-cmc-snmp.tem      \
		profile-cmc-tacacs.tem    \
		profile-cmc-upgrade.tem   \
		profile-cmc-users.tem     \
		profile-cmc-web.tem 	  \

endif

ifneq (${PROD_FEATURE_CMC_CLIENT},0)
	FILES+=                           \
		setup-cmc-client.tem      \

endif

ifneq (${PROD_FEATURE_CRYPTO},0)
	FILES+=                           \
		setup-ipsec.tem      \

endif

ifneq (${PROD_FEATURE_VIRT},0)
	FILES+=                          \
		mon-virt.tem             \
		mon-virt-common.tem      \
		mon-virt-detail.tem      \
		get-mon-virt.tem         \
		setup-virt.tem           \

endif

include ${PROD_TREE_ROOT}/src/mk/files.mk

DO_FILES_POST_INSTALL=\
        @templs=`echo ${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}/opt/tms${FILES_INSTALL_DIR}/*.tem` ;\
        for templ in $${templs}; do  \
		cmd="${PROD_BUILD_OUTPUT_DIR}/host/${BIN_DIR}/tmplcompiler/tmplcompiler $$templ" ; \
		if test ! "${PROD_BUILD_VERBOSE}" -eq 1; then \
	                echo "Compiling $${templ}" ; \
		else \
			echo "$${cmd}"; \
		fi ; \
                $${cmd} || exit 1; \
        done

