/*
 * @file diameter_platfrom.h
 * @brief
 * diameter_defs.h - Diameter Base Protocol definitions
 *
 * Vitaly Dzhitenov, July 2011
 *
 * Copyright (c) 2011-2013, Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef _DIAMETER_DEFS_H_
#define _DIAMETER_DEFS_H_

#define DIAMETER_PRODUCT_NAME   "Juniper Diameter Client"

/* firmware revision */
enum diameter_firmware_revision_enum
{
    DIAMETER_FIRMWARE_REVISION      = 0
};


static inline uint32_t diameter_message_version(diameter_message_header_t* header)
{
    return get_long (&header->message_version_length) & DIAMETER_MASK_MESSAGE_VERSION;
}
static inline uint32_t diameter_message_length(diameter_message_header_t* header)
{
    return get_long (&header->message_version_length) & DIAMETER_MASK_MESSAGE_LENGTH;
}
static inline uint32_t diameter_command_flags(diameter_message_header_t* header)
{
    return get_long (&header->command_flags_code) & DIAMETER_MASK_COMMAND_FLAGS;
}
static inline uint32_t diameter_command_code(diameter_message_header_t* header)
{
    return get_long (&header->command_flags_code) & DIAMETER_MASK_COMMAND_CODE;
}
static inline uint32_t diameter_application_id(diameter_message_header_t* header)
{
    return get_long (&header->application_id);
}
static inline uint32_t diameter_hop_by_hop(diameter_message_header_t* header)
{
    return get_long (&header->hop_by_hop_identifier);;
}
static inline uint32_t diameter_end_to_end(diameter_message_header_t* header)
{
    return get_long (&header->end_to_end_identifier);
}
static inline void diameter_get_message_info (const uint8_t* message,
                                              diameter_message_info_t* info)
{
    const diameter_message_header_t*  header = (const diameter_message_header_t*) message;
    uint32_t                    message_version_length;
    uint32_t                    command_flags_code;

    message_version_length = get_long (&header->message_version_length);
    command_flags_code = get_long (&header->command_flags_code);

	info->message_version = message_version_length & DIAMETER_MASK_MESSAGE_VERSION;
	info->message_length = message_version_length & DIAMETER_MASK_MESSAGE_LENGTH;
	info->command_flags = command_flags_code & DIAMETER_MASK_COMMAND_FLAGS;
	info->command_code = command_flags_code & DIAMETER_MASK_COMMAND_CODE;
	info->application_id = get_long (&header->application_id);
    info->hop_by_hop = get_long (&header->hop_by_hop_identifier);
    info->end_to_end = get_long (&header->end_to_end_identifier);
}


#endif
