#include "sysconfig.h"
#include "sysdeps.h"

#include "parallel.h"

int isprinter(void)
{
	return 0;
}

void doprinter(uae_u8)
{
}

void flushprinter(void)
{
}

void closeprinter(void)
{
}

int isprinteropen(void)
{
	return 0;
}

void initparallel(void)
{
}

int parallel_direct_write_data(uae_u8, uae_u8)
{
	return 0;
}

int parallel_direct_read_data(uae_u8 *)
{
	return 0;
}

int parallel_direct_write_status(uae_u8, uae_u8)
{
	return 0;
}

int parallel_direct_read_status(uae_u8 *)
{
	return 0;
}
