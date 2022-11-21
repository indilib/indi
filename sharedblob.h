#ifdef INDI_SHARED_BLOB_SUPPORT
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Allocate a buffer suitable for fast exchange over local links. Warning : the buffer will be sealed (readonly) once exchanged.
 *  \param size_t size of the memory area to allocate
 */
extern void * IDSharedBlobAlloc(size_t size);

/**
 * Attach to a received shared buffer by ID
 * The returned buffer cannot be realloced or sealed.
 * \return null on error + errno (invalid fd / system resources)
 */
extern void * IDSharedBlobAttach(int fd, size_t size);

/** \brief Free a buffer allocated using IDSharedBlobAlloc. Fall back to free for buffer that are not shared blob
 * Must be used for IBLOB.data
 */
extern void IDSharedBlobFree(void * ptr);

/** \brief Dettach a blob, but don't close its FD
 */
extern void IDSharedBlobDettach(void * ptr);

/** \brief Adjust the size of a buffer obtained using IDSharedBlobAlloc.
 *  \param size_t size of the memory area to allocate
 */
extern void * IDSharedBlobRealloc(void * ptr, size_t size);

/** \brief Return the filedescriptor backing up the given shared buffer.
 *  \return the filedescriptor or -1 if not a shared buffer pointer
 */
extern int IDSharedBlobGetFd(void * ptr);

/** \brief Seal (make readonly) a buffer allocated using IDSharedBlobAlloc. This is automatic when IDNewBlob
 *  \param size_t size of the memory area to allocate
 */
extern void IDSharedBlobSeal(void * ptr);

#ifdef __cplusplus
}
#endif

void * IDSharedBlobAlloc(size_t size);

#endif
