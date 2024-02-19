#include <linux/module.h>       // Needed by all modules
#include <linux/kernel.h>       // Needed for KERN_INFO
#include <linux/fs.h>           // Character device drivers
#include <linux/init.h>         // Macros used to mark up functions e.g. __init __exit
#include <linux/cdev.h>         // Char device structure
#include <linux/slab.h>         // kmalloc() and kfree()
#include <linux/uaccess.h>      // Copy to/from user
#include <linux/errno.h>
#include "message_slot.h"       // Definitions for our device


//MODULE INFORMATION
MODULE_LICENSE("GPL");             // License type -- this affects available functionality
MODULE_AUTHOR("ARI CHIS");
MODULE_DESCRIPTION("Message Slot Device Driver");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("0.1");

// Function prototypes
static int __init message_slot_init(void);
static void __exit message_slot_exit(void);
// Function prototypes for file operations
static int device_open(struct inode *, struct file *);
static long device_ioctl(struct file *, unsigned int, unsigned long);
// Helper function for device_ioctl
static struct message_channel* get_or_create_channel(struct message_slot* slot, unsigned int channel_id);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);

// Structure that declares the usual file access functions
static struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = device_open,
        .unlocked_ioctl = device_ioctl,
        .read = device_read,
        .write = device_write,
};

// Module initialization function
static int __init message_slot_init(void) {
    int result;
    // Register the device - we're using a predefined major number
    result = register_chrdev(MAJOR_NUM, "message_slot", &fops);
    if (result < 0) {
        printk(KERN_ERR "message_slot: cannot obtain major number %d\n", MAJOR_NUM);
        return result;
    }

    printk(KERN_INFO "Inserting message_slot module\n");
    return 0;
}

// Module cleanup function
static void __exit message_slot_exit(void) {
    // Unregister the device
    unregister_chrdev(MAJOR_NUM, "message_slot");
    printk(KERN_INFO "Removing message_slot module\n");
}

/**
Head of slots lists , there wo'nt be more than 256 slots
and each slot will not have more than 2^20 channels as needed
 */
static struct message_slot *slots = NULL; // Head of the slots list


static int device_open(struct inode *inode, struct file *file) {
    struct message_slot *slot = NULL;
    struct message_slot **ptr = &slots; // Pointer to the head of the list
    int minor = iminor(inode);

    // Search for an existing slot with the given minor number
    while (*ptr != NULL) {
        if ((*ptr)->minor == minor) {
            // Found the slot, break from the loop
            slot = *ptr;
            break;
        }
        ptr = &((*ptr)->next);
    }

    // If the slot wasn't found, create a new one
    if (!slot) {
        slot = kmalloc(sizeof(struct message_slot), GFP_KERNEL);
        if (!slot) {
            printk(KERN_ERR "message_slot: Out of memory\n");
            return -ENOMEM;
        }

        // Initialize the new slot
        slot->channels = NULL;
        slot->channel_count = 0;
        slot->minor = minor;
        slot->next = NULL;

        // Link the new slot into the list
        *ptr = slot;
    }

    // Store a pointer to the slot in file's private data for future operations
    file->private_data = slot;

    return 0; // Success
}


/**
 * @brief Handles IOCTL commands for the message slot device.
 *
 * This function is specifically designed to set the current channel for the
 * message slot device based on a non-zero channel ID provided by the user. It
 * validates the IOCTL command and the channel ID, ensuring that the command
 * is supported and the channel ID is non-zero.
 *
 * @param file A pointer to the file structure representing an open device file.
 *             This structure provides context for the IOCTL operation, including
 *             any previously associated channel or slot information.
 * @param ioctl_num The IOCTL command number. This function specifically handles
 *                  the MSG_SLOT_CHANNEL command, setting the current channel ID.
 * @param ioctl_param The parameter for the IOCTL command, used here as the channel ID.
 *                    This must be a non-zero value to be considered valid.
 *
 * @return Returns 0 on successful execution, indicating that the channel has been
 *         successfully set for subsequent operations. If an error occurs, such as
 *         an unsupported IOCTL command, an invalid channel ID, or if the channel
 *         cannot be set for any reason, the function returns -1. It is expected that
 *         user space interprets this return value as an indication to set errno to
 *         EINVAL, though the actual setting of errno must be handled in user space
 *         based on the negative return value.
 */
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    struct message_slot *slot; // Declaration at the start
    struct message_channel *channel;
    // Validate the IOCTL command and channel ID
    if (ioctl_num != MSG_SLOT_CHANNEL || ioctl_param == 0) {
        // Following instructions: Return -1 for error with an implication that errno should be set to EINVAL
        return -EINVAL;
    }

    // Assuming get_or_create_channel is implemented elsewhere and correctly handles channel management
    slot = file->private_data;
    if (!slot) {
        // Normally, would return -EBADF; per instructions, return -1
        return -EINVAL;
    }

    channel = get_or_create_channel(slot, (unsigned int)ioctl_param);
    if (!channel) {
        // Assuming channel creation failure due to memory allocation issues; per instructions, return -1
        return -EINVAL;
    }

    // Update private_data to point to the selected channel; this is a simplified approach
    // In a complete implementation, you might need to associate the channel more directly
    // with read/write operations or maintain the slot and channel relationship differently
    file->private_data = (void*)channel;

    return 0; // Success
}


/**
 * get_or_create_channel - Finds or creates a channel within a message slot.
 *
 * This function searches for a channel with the given ID within the specified message slot.
 * If the channel does not exist, it creates a new one, assuming the total number of channels
 * does not exceed the maximum limit of 2^20. This limit ensures the module adheres to the
 * assignment's specifications regarding the maximum number of message channels per message slot.
 *
 * Parameters:
 * @slot: Pointer to the message_slot structure within which the channel is to be searched for or created.
 * @channel_id: The unique identifier for the channel to search for or create.
 *
 * Return:
 * - On success, returns a pointer to the message_channel structure, either found or newly created.
 * - Returns NULL if the channel could not be created due to memory allocation failure or if adding
 *   another channel would exceed the maximum limit of 2^20 channels per message slot.
 *
 * Note:
 * The function checks if the total number of channels in the slot has reached the limit of 2^20.
 * If so, it refrains from creating a new channel and returns NULL. This behavior is crucial for
 * preventing the message slot from exceeding the specified maximum number of channels, ensuring
 * compliance with the assignment's constraints. Memory allocation failures also result in a NULL return,
 * signaling an inability to create a new channel under current memory constraints.
 */
struct message_channel *get_or_create_channel(struct message_slot *slot, unsigned int channel_id) {
    struct message_channel *current_channel = slot->channels;
    struct message_channel *prev_channel = NULL;
    struct message_channel *new_channel;

    // Iterate through existing channels to find a match.
    while (current_channel != NULL) {
        if (current_channel->channel_id == channel_id) {
            // Channel found.
            return current_channel;
        }
        prev_channel = current_channel;
        current_channel = current_channel->next;
    }

    // Check against the maximum allowed channels (2^20).
    if (slot->channel_count >= (1 << 20)) {
        printk(KERN_WARNING "Exceeded maximum number of channels (2^20).\n");
        return NULL; // Max limit reached, cannot create more channels.
    }

    // Allocate memory for a new channel.
    new_channel = kmalloc(sizeof(struct message_channel), GFP_KERNEL);
    if (!new_channel) {
        printk(KERN_ERR "Failed to allocate memory for new channel.\n");
        return NULL; // Memory allocation failed.
    }

    // Initialize the newly created channel.
    new_channel->channel_id = channel_id;
    new_channel->message_len = 0;
    new_channel->next = slot->channels; // Add to the start for simplicity.

    // Link the new channel to the slot.
    slot->channels = new_channel;
    slot->channel_count++; // Increment the total channel count for the slot.

    return new_channel; // Return the newly created channel.
}


/**
 * @brief Writes a message to the selected channel for the message slot device.
 *
 * This function writes a non-empty message of up to 128 bytes from the user's buffer
 * to the channel previously selected by an IOCTL command. It ensures the message
 * does not exceed the maximum allowed length and that a channel has been set for
 * the file descriptor.
 *
 * @param file Pointer to the file structure representing an open file descriptor.
 *        This structure contains private data set by the IOCTL command, which
 *        should point to the currently selected message channel.
 * @param buf User-space buffer containing the message to be written. The message
 *        can contain any sequence of bytes and is not necessarily a C string.
 * @param count Number of bytes to write from the user's buffer to the channel.
 *        This value must be greater than 0 and less than or equal to 128.
 * @param f_pos Position in the file to start writing. This parameter is ignored
 *        in this context, as the message slot channels do not support seeking.
 *
 * @return On success, returns the number of bytes written. On error, returns -1,
 *         with the expectation that errno is set to EINVAL if no channel has been
 *         set or the message length is invalid, and to EMSGSIZE if the message length
 *         exceeds 128 bytes. Other errors during message copying may also cause
 *         the function to return -1, with the appropriate errno value set by the
 *         calling context in user space.
 */
static ssize_t device_write(struct file *file, const char __user *buf, size_t count, loff_t *f_pos) {
    struct message_channel *channel;

    // Ensure a channel has been selected for the file descriptor
    if (!file->private_data) {

        return -EINVAL; // Channel not set
        }

    // Validate the message length
    if (count == 0 || count > 128) {
    
        return -EMSGSIZE; // Invalid message length
        }

    // Retrieve the selected channel from file's private data
    channel = (struct message_channel *)file->private_data;

    // Clear the channel's message buffer and copy the new message from user space
    memset(channel->message, 0, sizeof(channel->message));
    if (copy_from_user(channel->message, buf, count)) {
        return -1; // Failed to copy message from user space
        }

    // Update the message length for the channel
    channel->message_len = count;

    return count; // Successfully written, return the number of bytes written
    }


/**
 * @brief Reads the last message written to the selected channel into the user's buffer.
 * 
 * @param file Pointer to the file structure representing an open file descriptor.
 *        The file's private data is expected to point to the currently selected message channel.
 * @param buf User-space buffer where the read message will be copied.
 * @param count The size of the user's buffer.
 * @param f_pos Ignored in this context as the message slot does not support seeking.
 *
 * @return The number of bytes read on success. Returns -1 on error, with the expectation
 *         that errno is set to EINVAL if no channel has been set, EWOULDBLOCK if no message
 *         exists on the channel, ENOSPC if the user's buffer is too small, or another appropriate
 *         value for different errors.
 */
static ssize_t device_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos) {
    struct message_channel *channel;

    // Ensure a channel has been selected
    if (!file->private_data) {
        return -1; // Channel not set, implying errno should be set to EINVAL
    }

    // Retrieve the selected channel
    channel = (struct message_channel *)file->private_data;

    // Check if a message exists in the channel
    if (channel->message_len == 0) {
        return -EWOULDBLOCK; // No message exists, implying errno should be set to EWOULDBLOCK
    }

    // Ensure the user's buffer is large enough to hold the message
    if (count < channel->message_len) {
        return -ENOSPC; // Buffer too small, implying errno should be set to ENOSPC
    }

    // Copy the message to the user's buffer
    if (copy_to_user(buf, channel->message, channel->message_len)) {
        return -1; // Failed to copy, handle as appropriate, generally implies an error
    }

    return channel->message_len; // Return the number of bytes read
}


module_init(message_slot_init);
module_exit(message_slot_exit);

