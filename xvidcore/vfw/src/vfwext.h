/*

experimental vfw-api-extensions.
(c)2003, peter ross <pross@xvid.org>

*/

#ifndef _VFWEXT_H_
#define _VFWEXT_H_

/* VFWEXT */

#define VFWEXT_FOURCC   0xFFFFFFFF

typedef struct {
    DWORD   ciSize;       /* structure size */
    LONG    ciWidth;      /* frame width pixels */
    LONG    ciHeight;     /* frame height pixels */
    DWORD   ciRate;       /* frame rate/scale */
    DWORD   ciScale;      
    LONG    ciActiveFrame;  /* currently selected frame# */
    LONG    ciFrameCount;   /* total frames */
} VFWEXT_CONFIGURE_INFO_T;
#define VFWEXT_CONFIGURE_INFO   1


#endif  /* _VFWEXT_H_ */