// SPDX-License-Identifier: GPL-2.0-only
/* Send an SMS through the stock Quectel RIL API used by sample_sms. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
	QL_SMS_FORMAT_GSM7 = 0,
	QL_SMS_PHONE_SIZE = 256,
	QL_SMS_CONTENT_SIZE = 1440,
};

struct ql_sms_message {
	int32_t format;
	char phone_number[QL_SMS_PHONE_SIZE];
	int32_t content_length;
	uint8_t content[QL_SMS_CONTENT_SIZE];
};

extern int ql_sms_init(int sim_id);
extern int ql_sms_release(void);
extern int ql_sms_send_msg(struct ql_sms_message *message);

_Static_assert(sizeof(struct ql_sms_message) == 1704, "qlril SMS ABI size");
_Static_assert(offsetof(struct ql_sms_message, content_length) == 260,
	       "qlril SMS ABI length offset");
_Static_assert(offsetof(struct ql_sms_message, content) == 264,
	       "qlril SMS ABI content offset");

int main(int argc, char **argv)
{
	struct ql_sms_message message = { .format = QL_SMS_FORMAT_GSM7 };
	size_t number_len, content_len;
	int init_ret, send_ret, release_ret;

	if (argc != 3) {
		fprintf(stderr, "usage: %s NUMBER TEXT\n", argv[0]);
		return 2;
	}

	number_len = strlen(argv[1]);
	content_len = strlen(argv[2]);
	if (!number_len || number_len >= sizeof(message.phone_number) ||
	    !content_len || content_len > 160 ||
	    content_len > sizeof(message.content)) {
		fprintf(stderr, "invalid SMS number or content length\n");
		return 2;
	}

	memcpy(message.phone_number, argv[1], number_len + 1);
	memcpy(message.content, argv[2], content_len);
	message.content_length = (int32_t)content_len;

	init_ret = ql_sms_init(0);
	if (init_ret) {
		fprintf(stderr, "ql_sms_init failed: 0x%08x\n", init_ret);
		return 3;
	}

	send_ret = ql_sms_send_msg(&message);
	release_ret = ql_sms_release();
	if (send_ret) {
		fprintf(stderr, "ql_sms_send_msg failed: 0x%08x\n", send_ret);
		return 4;
	}
	if (release_ret) {
		fprintf(stderr, "ql_sms_release failed: 0x%08x\n", release_ret);
		return 5;
	}

	puts("SMS submitted through official qlril API");
	return 0;
}
