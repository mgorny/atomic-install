/* libcopy -- merge process
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_MERGE_H
#define _ATOMIC_INSTALL_MERGE_H

#include "journal.h"

enum ai_merge_stage {
	AI_MERGE_COPY_NEW = 0,
	AI_MERGE_BACKUP_OLD,
	AI_MERGE_REPLACE,
	AI_MERGE_CLEANUP
};

int ai_merge_copy_new(const char *source, const char *dest, ai_journal_t j);

#endif /*_ATOMIC_INSTALL_MERGE_H*/
