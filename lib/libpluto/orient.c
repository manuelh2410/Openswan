/* how to arrange connections: which end am I?
 * Copyright (C) 2015 Michael Richardson <mcr@xelerance.com>
 *
 * based upon ../../programs/pluto/initiate.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>

#include <openswan.h>
#include <openswan/ipsec_policy.h>
#include "openswan/pfkeyv2.h"

#include "sysdep.h"
#include "constants.h"
#include "oswalloc.h"
#include "oswtime.h"
#include "oswlog.h"

#include "pluto/server.h"
#include "pluto/connections.h"	/* needs id.h */

bool
orient(struct connection *c, unsigned int pluto_port)
{
    struct spd_route *sr;

    if (!oriented(*c))
    {
	struct iface_port *p;

	for (sr = &c->spd; sr; sr = sr->next)
	{
	    /* There can be more then 1 spd policy associated - required
	     * for cisco split networking when remote_peer_type=cisco
	     */
	    if(c->remotepeertype == CISCO && sr != &c->spd ) continue;

	    /* Note: this loop does not stop when it finds a match:
	     * it continues checking to catch any ambiguity.
	     */
	    for (p = interfaces; p != NULL; p = p->next)
	    {
              DBG_log("checking against if: %s", p->ip_dev->id_rname);
#ifdef NAT_TRAVERSAL
		if (p->ike_float) continue;
#endif

#ifdef HAVE_LABELED_IPSEC
		if (c->loopback && sameaddr(&sr->this.host_addr, &p->ip_addr)) {
		DBG(DBG_CONTROLMORE,
			DBG_log("loopback connections \"%s\" with interface %s!"
			 , c->name, p->ip_dev->id_rname));
			c->interface = p;
			break;
		}
#endif

		for (;;)
		{
		    /* check if this interface matches this end */
		    if (sameaddr(&sr->this.host_addr, &p->ip_addr)
			&& (kern_interface != NO_KERNEL
			    || sr->this.host_port == pluto_port))
		    {
			if (oriented(*c))
			{
			    if (c->interface->ip_dev == p->ip_dev)
				loglog(RC_LOG_SERIOUS
				       , "both sides of \"%s\" are our interface %s!"
				       , c->name, p->ip_dev->id_rname);
			    else
				loglog(RC_LOG_SERIOUS, "two interfaces match \"%s\" (%s, %s)"
				       , c->name, c->interface->ip_dev->id_rname, p->ip_dev->id_rname);
			    terminate_connection(c->name);
			    c->interface = NULL;	/* withdraw orientation */
			    return FALSE;
			}
			c->interface = p;
		    }

		    /* done with this interface if it doesn't match that end */
		    if (!(sameaddr(&sr->that.host_addr, &p->ip_addr)
			  && (kern_interface!=NO_KERNEL
			      || sr->that.host_port == pluto_port)))
			break;

		    /* swap ends and try again.
		     * It is a little tricky to see that this loop will stop.
		     * Only continue if the far side matches.
		     * If both sides match, there is an error-out.
		     */
		    {
			struct end t = sr->this;

			sr->this = sr->that;
			sr->that = t;
		    }
		}
	    }
	}
    }
    return oriented(*c);
}

/*
 * Local Variables:
 * c-basic-offset:4
 * c-style: pluto
 * End:
 */