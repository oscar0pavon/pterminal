#ifndef WSELECTION_H
#define WSELECTION_H

#include "primary_selection.h"

typedef struct PrimarySelection{
  struct zwp_primary_selection_device_v1 *device;
  struct zwp_primary_selection_offer_v1 *offer;
}PrimarySelection;

extern PrimarySelection primary_selection;

void configure_selection(void);

#endif
