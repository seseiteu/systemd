/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright (C) 2013 Intel Corporation. All rights reserved.

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "dhcp-internal.h"

int dhcp_option_append(uint8_t options[], size_t size, size_t *offset,
                       uint8_t code, size_t optlen, const void *optval) {
        assert(options);
        assert(offset);

        switch (code) {

        case DHCP_OPTION_PAD:
        case DHCP_OPTION_END:
                if (size - *offset < 1)
                        return -ENOBUFS;

                options[*offset] = code;
                *offset += 1;
                break;

        default:
                if (size - *offset < optlen + 2)
                        return -ENOBUFS;

                assert(optval);

                options[*offset] = code;
                options[*offset + 1] = optlen;
                memcpy(&options[*offset + 2], optval, optlen);

                *offset += optlen + 2;

                break;
        }

        return 0;
}

static int parse_options(const uint8_t options[], size_t buflen, uint8_t *overload,
                         uint8_t *message_type, dhcp_option_cb_t cb,
                         void *user_data) {
        uint8_t code, len;
        size_t offset = 0;

        while (offset < buflen) {
                switch (options[offset]) {
                case DHCP_OPTION_PAD:
                        offset++;

                        break;

                case DHCP_OPTION_END:
                        return 0;

                case DHCP_OPTION_MESSAGE_TYPE:
                        if (buflen < offset + 3)
                                return -ENOBUFS;

                        len = options[++offset];
                        if (len != 1)
                                return -EINVAL;

                        if (message_type)
                                *message_type = options[++offset];
                        else
                                offset++;

                        offset++;

                        break;

                case DHCP_OPTION_OVERLOAD:
                        if (buflen < offset + 3)
                                return -ENOBUFS;

                        len = options[++offset];
                        if (len != 1)
                                return -EINVAL;

                        if (overload)
                                *overload = options[++offset];
                        else
                                offset++;

                        offset++;

                        break;

                default:
                        if (buflen < offset + 3)
                                return -ENOBUFS;

                        code = options[offset];
                        len = options[++offset];

                        if (buflen < ++offset + len)
                                return -EINVAL;

                        if (cb)
                                cb(code, len, &options[offset], user_data);

                        offset += len;

                        break;
                }
        }

        if (offset < buflen)
                return -EINVAL;

        return 0;
}

int dhcp_option_parse(DHCPMessage *message, size_t len,
                      dhcp_option_cb_t cb, void *user_data) {
        uint8_t overload = 0;
        uint8_t message_type = 0;
        int r;

        if (!message)
                return -EINVAL;

        if (len < sizeof(DHCPMessage))
                return -EINVAL;

        len -= sizeof(DHCPMessage);

        r = parse_options(message->options, len, &overload, &message_type,
                          cb, user_data);
        if (r < 0)
                return r;

        if (overload & DHCP_OVERLOAD_FILE) {
                r = parse_options(message->file, sizeof(message->file),
                                NULL, &message_type, cb, user_data);
                if (r < 0)
                        return r;
        }

        if (overload & DHCP_OVERLOAD_SNAME) {
                r = parse_options(message->sname, sizeof(message->sname),
                                NULL, &message_type, cb, user_data);
                if (r < 0)
                        return r;
        }

        if (message_type)
                return message_type;

        return -ENOMSG;
}
