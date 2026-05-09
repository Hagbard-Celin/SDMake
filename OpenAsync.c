/*
 *
 *  This file is a local copy of "AsyncIO/src/OpenAsync.c" to fix a double-
 *  close bug, while keeping original unchanged.
 *
 *  My change here is just deleting four lines of trivial code, which is not
 *  copyrightable in any jurisdiction.
 *
 *  - Hagbard Celine
 *
 */

#include "AsyncIO/src/async.h"


#ifdef ASIO_NOEXTERNALS
_LIBCALL AsyncFile *
OpenAsync(
	_REG( a0 ) const STRPTR fileName,
	_REG( d0 ) OpenModes mode,
	_REG( d1 ) LONG bufferSize,
	_REG( a1 ) struct ExecBase *SysBase,
	_REG( a2 ) struct DosLibrary *DOSBase )
#else
_LIBCALL AsyncFile *
OpenAsync(
	_REG( a0 ) const STRPTR fileName,
	_REG( d0 ) OpenModes mode,
	_REG( d1 ) LONG bufferSize )
#endif
{
	static const WORD PrivateOpenModes[] =
	{
		MODE_OLDFILE, MODE_NEWFILE, MODE_READWRITE
	};
	BPTR		handle;
	AsyncFile	*file = NULL;

	if( handle = Open( fileName, PrivateOpenModes[ mode ] ) )
	{
#ifdef ASIO_NOEXTERNALS
		file = AS_OpenAsyncFH( handle, mode, bufferSize, TRUE, SysBase, DOSBase );
#else
		file = AS_OpenAsyncFH( handle, mode, bufferSize, TRUE );
#endif
	}

	return( file );
}

