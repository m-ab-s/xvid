#ifndef _IXVID_H_
#define _IXVID_H_

#ifdef __cplusplus
extern "C" {
#endif


DEFINE_GUID(IID_IXvidDecoder,
	0x00000000, 0x4fef, 0x40d3, 0xb3, 0xfa, 0xe0, 0x53, 0x1b, 0x89, 0x7f, 0x98);

DECLARE_INTERFACE_(IXvidDecoder, IUnknown)
{
};



#ifdef __cplusplus
}
#endif

#endif /* _IXVID_H_ */