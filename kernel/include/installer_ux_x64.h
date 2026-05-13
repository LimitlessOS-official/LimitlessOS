#ifndef LIMITLESS_INSTALLER_UX_X64_H
#define LIMITLESS_INSTALLER_UX_X64_H

#include "types.h"

#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
void installer_ux64_init(void);
u32 installer_ux64_product(void);
u32 installer_ux64_welcome(void);
u32 installer_ux64_beginner_mode(void);
u32 installer_ux64_advanced_mode(void);
u32 installer_ux64_hardware_summary(void);
u32 installer_ux64_recommendation(void);
u32 installer_ux64_component_selection(void);
u32 installer_ux64_unavailable_components_labeled(void);
u32 installer_ux64_account_page(void);
u32 installer_ux64_personal_unavailable(void);
u32 installer_ux64_enterprise_unavailable(void);
u32 installer_ux64_cloud_page(void);
u32 installer_ux64_cloud_sync_unavailable(void);
u32 installer_ux64_ai_page(void);
u32 installer_ux64_ai_setup_unavailable(void);
u32 installer_ux64_plan_generated(void);
u32 installer_ux64_dryrun_no_writes(void);
u32 installer_ux64_forbidden_target_denied(void);
u32 installer_ux64_write_action_denied(void);
u32 installer_ux64_format_action_denied(void);
u32 installer_ux64_boot_entry_denied(void);
u32 installer_ux64_package_install_denied(void);
u32 installer_ux64_cloud_enable_denied(void);
u32 installer_ux64_ai_enable_denied(void);
u32 installer_ux64_no_ambient_installer(void);
u32 installer_ux64_no_ambient_storage(void);
u32 installer_ux64_no_ambient_firmware(void);
u32 installer_ux64_no_ambient_package(void);
u32 installer_ux64_no_ambient_identity_cloud_secret(void);
u32 installer_ux64_writes_planned(void);
u32 installer_ux64_formats_planned(void);
u32 installer_ux64_boot_entries_planned(void);
u32 installer_ux64_package_ops_planned(void);
u32 installer_ux64_real_install_approved(void);
const char *installer_ux64_mode(void);
const char *installer_ux64_selected_profile(void);
const char *installer_ux64_recommendation_text(void);
const char *installer_ux64_component_status(void);
const char *installer_ux64_account_status(void);
const char *installer_ux64_cloud_status(void);
const char *installer_ux64_ai_status(void);
const char *installer_ux64_plan_status(void);
const char *installer_ux64_dryrun_status(void);
#else
static inline void installer_ux64_init(void) {}
static inline u32 installer_ux64_product(void) { return 0u; }
static inline u32 installer_ux64_welcome(void) { return 0u; }
static inline u32 installer_ux64_beginner_mode(void) { return 0u; }
static inline u32 installer_ux64_advanced_mode(void) { return 0u; }
static inline u32 installer_ux64_hardware_summary(void) { return 0u; }
static inline u32 installer_ux64_recommendation(void) { return 0u; }
static inline u32 installer_ux64_component_selection(void) { return 0u; }
static inline u32 installer_ux64_unavailable_components_labeled(void) { return 1u; }
static inline u32 installer_ux64_account_page(void) { return 0u; }
static inline u32 installer_ux64_personal_unavailable(void) { return 1u; }
static inline u32 installer_ux64_enterprise_unavailable(void) { return 1u; }
static inline u32 installer_ux64_cloud_page(void) { return 0u; }
static inline u32 installer_ux64_cloud_sync_unavailable(void) { return 1u; }
static inline u32 installer_ux64_ai_page(void) { return 0u; }
static inline u32 installer_ux64_ai_setup_unavailable(void) { return 1u; }
static inline u32 installer_ux64_plan_generated(void) { return 0u; }
static inline u32 installer_ux64_dryrun_no_writes(void) { return 1u; }
static inline u32 installer_ux64_forbidden_target_denied(void) { return 1u; }
static inline u32 installer_ux64_write_action_denied(void) { return 1u; }
static inline u32 installer_ux64_format_action_denied(void) { return 1u; }
static inline u32 installer_ux64_boot_entry_denied(void) { return 1u; }
static inline u32 installer_ux64_package_install_denied(void) { return 1u; }
static inline u32 installer_ux64_cloud_enable_denied(void) { return 1u; }
static inline u32 installer_ux64_ai_enable_denied(void) { return 1u; }
static inline u32 installer_ux64_no_ambient_installer(void) { return 1u; }
static inline u32 installer_ux64_no_ambient_storage(void) { return 1u; }
static inline u32 installer_ux64_no_ambient_firmware(void) { return 1u; }
static inline u32 installer_ux64_no_ambient_package(void) { return 1u; }
static inline u32 installer_ux64_no_ambient_identity_cloud_secret(void) { return 1u; }
static inline u32 installer_ux64_writes_planned(void) { return 0u; }
static inline u32 installer_ux64_formats_planned(void) { return 0u; }
static inline u32 installer_ux64_boot_entries_planned(void) { return 0u; }
static inline u32 installer_ux64_package_ops_planned(void) { return 0u; }
static inline u32 installer_ux64_real_install_approved(void) { return 0u; }
static inline const char *installer_ux64_mode(void) { return "unavailable"; }
static inline const char *installer_ux64_selected_profile(void) { return "unavailable"; }
static inline const char *installer_ux64_recommendation_text(void) { return "unavailable"; }
static inline const char *installer_ux64_component_status(void) { return "unavailable"; }
static inline const char *installer_ux64_account_status(void) { return "local-only"; }
static inline const char *installer_ux64_cloud_status(void) { return "unavailable"; }
static inline const char *installer_ux64_ai_status(void) { return "unavailable"; }
static inline const char *installer_ux64_plan_status(void) { return "unavailable"; }
static inline const char *installer_ux64_dryrun_status(void) { return "no-writes"; }
#endif

#endif
