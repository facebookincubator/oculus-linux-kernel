/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FWUPDATE_DEBUG_H
#define _FWUPDATE_DEBUG_H

int fwupdate_create_debugfs(struct device *dev, const char * const flavor);
int fwupdate_remove_debugfs(struct device *dev);

#endif
