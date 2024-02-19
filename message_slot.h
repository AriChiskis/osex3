#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#define MAJOR_NUM 235
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)

struct message_channel {
    unsigned int channel_id;
    char message[128];
    size_t message_len;
    struct message_channel *next;
};

struct message_slot {
    struct message_channel *channels;
    unsigned long channel_count;
    int minor;
    struct message_slot *next;
};


#endif /* MESSAGE_SLOT_H */

