/*
 *
 * build_version.h
 *
 *
 *
 */

#ifndef __BUILD_VERSION_H_
#define __BUILD_VERSION_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file build_version.h Declaration of various build strings.
 * \ingroup lc
 *
 * As part of the build process, information about the build environment,
 * target platform, compiled in features, etc. are made available
 * via these build strings.
 *
 * Note: the variables declared here are actually defined in
 * build_version.c, which is automatically generated.
 */

/**
 * Absolute path to root of Samara tree from which image was built.
 * Must be set explicitly during the build.
 */
extern const char build_prod_tree_root[];

/**
 * Absolute path to root of customer-specific source tree;
 * defaults to $PROD_TREE_ROOT/customer.
 */
extern const char build_prod_customer_root[];

/**
 * Absolute path to root of build output tree;
 * defaults to $PROD_TREE_ROOT/output.
 */
extern const char build_prod_output_root[];

extern const char build_prod_root_prefix[];

/**
 * Date of build.  Format: "yyyy-mm-dd hh:mm:ss".
 */
extern const char build_date[];

extern const char build_type[];
extern const char build_number[];
extern const long int build_number_int;
extern const char build_id[];
extern const char build_scm_info[];
extern const char build_scm_info_short[];

/**
 * Source ID, a string describing the build source version textually.
 * Set from $PROD_TMS_SRCS_ID.
 */
extern const char build_tms_srcs_id[];
extern const char build_tms_srcs_release_id[];
extern const char build_tms_srcs_overlay_ids[];
extern const char build_tms_srcs_bundles[];

/**
 * Product version.
 */
extern const char build_tms_srcs_version[];
extern const char build_tms_srcs_date[];
extern const char build_tms_srcs_customer[];

/**
 * Hostname of system where this image was built.
 */
extern const char build_host_machine[];

/**
 * Username of user who ran the build.
 */
extern const char build_host_user[];
extern const char build_host_real_user[];
extern const char build_host_os[];
extern const char build_host_os_lc[];
extern const char build_host_platform[];
extern const char build_host_platform_lc[];
extern const char build_host_platform_version[];
extern const char build_host_platform_version_lc[];
extern const char build_host_platform_full[];
extern const char build_host_platform_full_lc[];
extern const char build_host_arch[];
extern const char build_host_arch_lc[];
extern const char build_host_cpu[];
extern const char build_host_cpu_lc[];
extern const char build_target_os[];
extern const char build_target_os_lc[];
extern const char build_target_platform[];
extern const char build_target_platform_lc[];
extern const char build_target_platform_version[];
extern const char build_target_platform_version_lc[];
extern const char build_target_platform_full[];
extern const char build_target_platform_full_lc[];
extern const char build_target_arch[];
extern const char build_target_arch_lc[];
extern const char build_target_arch_family[];
extern const char build_target_arch_family_lc[];
extern const char build_target_cpu[];
extern const char build_target_cpu_lc[];
extern const char build_target_hwname[];
extern const char build_target_hwname_lc[];
extern const char build_target_hwnames_compat[];
extern const char build_target_hwnames_compat_lc[];
extern const char build_prod_product[];
extern const char build_prod_product_lc[];
extern const char build_prod_customer[];
extern const char build_prod_customer_lc[];
extern const char build_prod_id[];
extern const char build_prod_id_lc[];
extern const char build_prod_name[];
extern const char build_prod_release[];
extern const char build_prod_version[];
extern const char build_prod_features_enabled[];
extern const char build_prod_features_enabled_lc[];
extern const char build_prod_locales[];

/* 
 * NOTE: Do not use this function, as it is internal.  Instead use
 * lc_leak_build_version_info() .
 *
 * Leak some memory so that a valgrind run will always have a mention
 * of our versioning information.  If 'keep_ref' is true, it'll be a
 * 'still reachable' leak, otherwise a 'definitely lost' leak.
 */
int bv_versions_leak_init(int keep_ref, unsigned long leak_size);

#ifdef __cplusplus
}
#endif

#endif /* __BUILD_VERSION_H_ */
