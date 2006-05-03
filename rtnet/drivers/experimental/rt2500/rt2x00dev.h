/* rt2x00dev.h 
 *
 * Copyright (C) 2004 - 2005 rt2x00-2.0.0-b3 SourceForge Project
 *	                     <http://rt2x00.serialmonkey.com>
 *               2006        rtnet adaption by Daniel Gregorek 
 *                           <dxg@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
  Module: rt2x00dev
  Abstract: Data structures and registers for the rt2x00 device modules.
  Supported chipsets: RT2460, RT2560 & RT2570.
*/

#ifndef RT2500DEV_H
#define RT2500DEV_H

/*
 * Register handlers.
 * We store the position of a register field inside a field structure,
 * This will simplify the process of setting and reading a certain field
 * inside the register.
 */
struct _rt2x00_field16{
    u16	bit_offset;
    u16	bit_mask;
} __attribute__ ((packed));

struct _rt2x00_field32{
    u32	bit_offset;
    u32	bit_mask;
} __attribute__ ((packed));

#define FIELD16(__offset, __mask)	( (struct _rt2x00_field16) { (__offset), (__mask) } )
#define FIELD32(__offset, __mask)	( (struct _rt2x00_field32) { (__offset), (__mask) } )

static inline void
rt2x00_set_field32(u32 *reg, const struct _rt2x00_field32 field, const u32 value) {

    *reg &= cpu_to_le32(~(field.bit_mask));
    *reg |= cpu_to_le32((value << field.bit_offset) & field.bit_mask);
}

static inline void
rt2x00_set_field32_nb(u32 *reg, const struct _rt2x00_field32 field, const u32 value) {

    *reg &= ~(field.bit_mask);
    *reg |= (value << field.bit_offset) & field.bit_mask;
}

static inline u32
rt2x00_get_field32(const u32 reg, const struct _rt2x00_field32 field) {

    return (le32_to_cpu(reg) & field.bit_mask) >> field.bit_offset;
}

static inline u32
rt2x00_get_field32_nb(const u32 reg, const struct _rt2x00_field32 field) {

    return (reg & field.bit_mask) >> field.bit_offset;
}

static inline void
rt2x00_set_field16(u16 *reg, const struct _rt2x00_field16 field, const u16 value) {

    *reg &= cpu_to_le16(~(field.bit_mask));
    *reg |= cpu_to_le16((value << field.bit_offset) & field.bit_mask);
}

static inline void
rt2x00_set_field16_nb(u16 *reg, const struct _rt2x00_field16 field, const u16 value) {

    *reg &= ~(field.bit_mask);
    *reg |= (value << field.bit_offset) & field.bit_mask;
}

static inline u16
rt2x00_get_field16(const u16 reg, const struct _rt2x00_field16 field) {

    return (le16_to_cpu(reg) & field.bit_mask) >> field.bit_offset;
}

static inline u16
rt2x00_get_field16_nb(const u16 reg, const struct _rt2x00_field16 field) {

    return (reg & field.bit_mask) >> field.bit_offset;
}

/*
 * rf register sructure for channel selection.
 */
struct _rf_channel{
    u32				rf1;
    u32				rf2;
    u32				rf3;
    u32				rf4;
}__attribute__ ((packed));

/*
 * Chipset identification
 * The chipset on the device is composed of a RT and RF chip.
 * The chipset combination is important for determining device capabilities.
 */
struct _rt2x00_chip{
    u16				rt;
    u16				rf;
} __attribute__ ((packed));

/*
 * Set chipset data.
 * Some rf values for RT2400 devices are equal to rf values for RT2500 devices.
 * To prevent problems, all rf values will be masked to clearly seperate each chipset.
 */
static inline void
set_chip(struct _rt2x00_chip *chipset, const u16 rt, const u16 rf) {

    INFO("Chipset detected - rt: %04x, rf: %04x.\n", rt, rf);

    chipset->rt = rt;
    chipset->rf = rf | (chipset->rt & 0xff00);
}

static inline char
rt2x00_rt(const struct _rt2x00_chip *chipset, const u16 chip) {

    return (chipset->rt == chip);
}

static inline char
rt2x00_rf(const struct _rt2x00_chip *chipset, const u16 chip) {

    return (chipset->rf == chip);
}

static inline u16
rt2x00_get_rf(const struct _rt2x00_chip *chipset) {

    return chipset->rf;
}

/*
 * _data_ring
 * Data rings are used by the device to send and receive packets.
 * The data_addr is the base address of the data memory.
 * Device specifice information is pointed to by the priv pointer.
 * The index values may only be changed with the functions ring_index_inc()
 * and ring_index_done_inc().
 */
struct _data_ring {

    /*
     * Base address of packet ring.
     */
    dma_addr_t			data_dma;
    void				*data_addr;

    /*
     * Private device specific data.
     */
    void				*priv;
    struct _rt2x00_device		*device;

    /*
     * Current index values.
     */
    u8				index;
    u8				index_done;

    /*
     * Ring type set with RING_* define.
     */
    u8				ring_type;

    /*
     * Number of entries in this ring.
     */
    u8				max_entries;

    /*
     * Size of packet and descriptor in bytes.
     */
    u16				entry_size;
    u16				desc_size;

    /*
     * Total allocated memory size.
     */
    u32				mem_size;
} __attribute__ ((packed));

/*
 * Number of entries in a packet ring.
 */
#define RX_ENTRIES			8
#define TX_ENTRIES			8
#define ATIM_ENTRIES			1
#define PRIO_ENTRIES			2
#define BEACON_ENTRIES			1

/*
 * Initialization and cleanup routines.
 */
static inline void rt2x00_init_ring(
                                    struct _rt2x00_device *device,
                                    struct _data_ring *ring,
                                    const u8 ring_type,
                                    const u16 max_entries,
                                    const u16 entry_size,
                                    const u16 desc_size) {

    ring->device = device;
    ring->index = 0;
    ring->index_done = 0;
    ring->ring_type = ring_type;
    ring->max_entries = max_entries;
    ring->entry_size = entry_size;
    ring->desc_size = desc_size;
    ring->mem_size = ring->max_entries * (ring->desc_size + ring->entry_size);
}

static inline void rt2x00_deinit_ring(struct _data_ring *ring) {

    ring->device = NULL;
    ring->index = 0;
    ring->index_done = 0;
    ring->ring_type = 0;
    ring->max_entries = 0;
    ring->entry_size = 0;
    ring->desc_size = 0;
    ring->mem_size = 0;
}

/*
 * Ring index manipulation functions.
 */
static inline void rt2x00_ring_index_inc(struct _data_ring *ring) {

    ring->index = (++ring->index < ring->max_entries) ? ring->index : 0;
}

static inline void rt2x00_ring_index_done_inc(struct _data_ring *ring) {

    ring->index_done = (++ring->index_done < ring->max_entries) ? ring->index_done : 0;
}

static inline void rt2x00_ring_clear_index(struct _data_ring *ring) {

    ring->index = 0;
    ring->index_done = 0;
}

static inline u8 rt2x00_ring_empty(struct _data_ring *ring) {

    return ring->index_done == ring->index;
}

static inline u8 rt2x00_ring_free_entries(struct _data_ring *ring) {

    if(ring->index >= ring->index_done)
        return ring->max_entries - (ring->index - ring->index_done);
    else
        return ring->index_done - ring->index;
}

/*
 * Return PLCP value matching the rate.
 * PLCP values according to ieee802.11a-1999 p.14.
 */
static inline u8 rt2x00_get_plcp(const u8 rate) {

    u8	counter = 0x00;
    u8	plcp[12] = {
        0x00, 0x01, 0x02, 0x03,					/* CCK. */
        0x0b, 0x0f, 0x0a, 0x0e, 0x09, 0x0d, 0x08, 0x0c,		/* OFDM. */
    };

    for(; counter < 12; counter++){
        if(capabilities.bitrate[counter] == rate)
            return plcp[counter];
    }

    return 0xff;
}

#define OFDM_CHANNEL(__channel)		( (__channel) >= CHANNEL_OFDM_MIN && (__channel) <= CHANNEL_OFDM_MAX )
#define UNII_LOW_CHANNEL(__channel)	( (__channel) >= CHANNEL_UNII_LOW_MIN && (__channel) <= CHANNEL_UNII_LOW_MAX )
#define HIPERLAN2_CHANNEL(__channel)	( (__channel) >= CHANNEL_HIPERLAN2_MIN && (__channel) <= CHANNEL_HIPERLAN2_MAX )
#define UNII_HIGH_CHANNEL(__channel)	( (__channel) >= CHANNEL_UNII_HIGH_MIN && (__channel) <= CHANNEL_UNII_HIGH_MAX )

/*
 * Return the index value of the channel starting from the first channel of the range.
 * Where range can be OFDM, UNII (low), HiperLAN2 or UNII (high).
 */
static inline int rt2x00_get_channel_index(const u8 channel) {

    if(OFDM_CHANNEL(channel))
        return (channel - 1);

    if(channel % 4)
        return -EINVAL;

    if(UNII_LOW_CHANNEL(channel))
        return ((channel - CHANNEL_UNII_LOW_MIN) / 4);
    else if(HIPERLAN2_CHANNEL(channel))
        return ((channel - CHANNEL_HIPERLAN2_MIN) / 4);
    else if(UNII_HIGH_CHANNEL(channel))
        return ((channel - CHANNEL_UNII_HIGH_MIN) / 4);
    return -EINVAL;
}

#endif
