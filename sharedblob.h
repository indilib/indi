#ifdef INDI_SHARED_BLOB_SUPPORT

#ifdef __cplusplus
extern "C" {
#endif

// FIXME: duplicated from indidevapi.h. Can we share ?


/** \brief Allocate a buffer suitable for fast exchange over local links. Warning : the buffer will be sealed (readonly) once exchanged.
    \param size_t size of the memory area to allocate
 */
extern void * IDSharedBlobAlloc(size_t size);

/** \brief Adjust the size of a buffer obtained using IDSharedBlobAlloc.
    \param size_t size of the memory area to allocate
 */
extern void * IDSharedBlobRealloc(void * ptr, size_t size);

/** \brief Dettach a blob, but don't close its FD
 */
extern void IDSharedBlobDettach(void * ptr);


#ifdef __cplusplus
}
#endif

void * IDSharedBlobAlloc(size_t size);

#endif
