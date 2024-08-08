
#ifndef _ETHER_H_
#define _ETHER_H_

#define RTE_ETHER_ADDR_LEN  6 /**< Length of Ethernet address. */

/**
 * Ethernet address:
 * A universally administered address is uniquely assigned to a device by its
 * manufacturer. The first three octets (in transmission order) contain the
 * Organizationally Unique Identifier (OUI). The following three (MAC-48 and
 * EUI-48) octets are assigned by that organization with the only constraint
 * of uniqueness.
 * A locally administered address is assigned to a device by a network
 * administrator and does not contain OUIs.
 * See http://standards.ieee.org/regauth/groupmac/tutorial.html
 */
struct rte_ether_addr {
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN]; /**< Addr bytes in tx order */
} __attribute__((aligned(2)));


/**
 * Ethernet header: Contains the destination address, source address
 * and frame type.
 */
struct rte_ether_hdr {
	struct rte_ether_addr d_addr; /**< Destination address. */
	struct rte_ether_addr s_addr; /**< Source address. */
	uint16_t ether_type;      /**< Frame type. */
} __attribute__((aligned(2)));

#endif


