# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

INSTALL_DIRECTIVES= \
  ${SYS_INSTALL_DIR_opt} \
  ${SYS_INSTALL_DIR_opt_dpianalyzer} \
  ${SYS_INSTALL_DIR_opt_dpianalyzer_scripts} \
  ${SYS_INSTALL_DIR_opt_dpianalyzer_defaultconfig} \
  ${SYS_INSTALL_FILE_gen_utils_py} \
  ${SYS_INSTALL_FILE_header_py} \
  ${SYS_INSTALL_FILE_log_analyzer_pbr_py} \
  ${SYS_INSTALL_FILE_mfc_accesslog_analyzer_py} \
  ${SYS_INSTALL_FILE_mfc_header_py} \
  ${SYS_INSTALL_FILE_clear_filter_rules_py} \
  ${SYS_INSTALL_FILE_mdreq_py} \
  ${SYS_INSTALL_FILE_installconf_sh} \
  ${SYS_INSTALL_FILE_analyzer_conf} \
  ${SYS_INSTALL_FILE_golden_config_txt} \
  ${SYS_INSTALL_FILE_override_rules_xml} \
  ${SYS_INSTALL_FILE_log_analyzer_server} \
  ${SYS_INSTALL_FILE_clear_filter_rules} \


CLEANFILES= \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/gen_utils.py \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/header.py \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/log_analyzer_pbr.py \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/mfc_accesslog_analyzer.py \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/mfc_header.py \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/clear_filter_rules.py \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/mdreq.py \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/defaultconfig/installconf.sh \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/defaultconfig/analyzer.conf \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/defaultconfig/golden_config.txt \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/defaultconfig/override_rules.xml \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/log_analyzer_server \
    ${FULL_INSTALL_DIR}/opt/dpi-analyzer/scripts/clear_filter_rules \


SYS_INSTALL_DIR_opt="dir 0755 root root /opt"
SYS_INSTALL_DIR_opt_dpianalyzer="dir 0755 root root /opt/dpi-analyzer"
SYS_INSTALL_DIR_opt_dpianalyzer_scripts="dir 0755 root root /opt/dpi-analyzer/scripts"
SYS_INSTALL_DIR_opt_dpianalyzer_defaultconfig="dir 0755 root root /opt/dpi-analyzer/defaultconfig"

SYS_INSTALL_FILE_gen_utils_py="file gen_utils.py 0444 root root /opt/dpi-analyzer/scripts/gen_utils.py"
SYS_INSTALL_FILE_header_py="file header.py 0444 root root /opt/dpi-analyzer/scripts/header.py"
SYS_INSTALL_FILE_log_analyzer_pbr_py="file log_analyzer_pbr.py 0555 root root /opt/dpi-analyzer/scripts/log_analyzer_pbr.py"
SYS_INSTALL_FILE_mfc_accesslog_analyzer_py="file mfc_accesslog_analyzer.py 0444 root root /opt/dpi-analyzer/scripts/mfc_accesslog_analyzer.py"
SYS_INSTALL_FILE_mfc_header_py="file mfc_header.py 0444 root root /opt/dpi-analyzer/scripts/mfc_header.py"
SYS_INSTALL_FILE_clear_filter_rules_py="file clear_filter_rules.py 0444 root root /opt/dpi-analyzer/scripts/clear_filter_rules.py"
SYS_INSTALL_FILE_mdreq_py="file mdreq.py 0444 root root /opt/dpi-analyzer/scripts/mdreq.py"
SYS_INSTALL_FILE_log_analyzer_server="file log_analyzer_server 0555 root root /opt/dpi-analyzer/scripts/log_analyzer_server"
SYS_INSTALL_FILE_clear_filter_rules="file clear_filter_rules 0555 root root /opt/dpi-analyzer/scripts/clear_filter_rules"

SYS_INSTALL_FILE_installconf_sh="file installconf.sh 0555 root root /opt/dpi-analyzer/defaultconfig/installconf.sh"
SYS_INSTALL_FILE_analyzer_conf="file analyzer.conf 0444 root root /opt/dpi-analyzer/defaultconfig/analyzer.conf"
SYS_INSTALL_FILE_golden_config_txt="file golden_config.txt 0444 root root /opt/dpi-analyzer/defaultconfig/golden_config.txt"
SYS_INSTALL_FILE_override_rules_xml="file override_rules.xml 0444 root root /opt/dpi-analyzer/defaultconfig/override_rules.xml"


include ${PROD_TREE_ROOT}/src/mk/files_adv.mk

