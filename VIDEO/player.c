#include <Windows.h>
#include <stdio.h>
#include <objidl.h>
#include <Aviriff.h>

typedef struct _CIRCULAR_BUFFER { /* FIFO */
	INT iSize;			/* The capacity, in elements, of the buffer */
	INT iRead;			/* The number of elements read from the buffer */
	INT iWritten;		/* The number of elements written to the buffer */
	DWORD dwRecSize;	/* The size, in bytes, of a record */
	LPBYTE lpBuffer;	/* A pointer to the actual buffer data */
} CIRCULAR_BUFFER;

typedef struct _DCMPRSSCTX {
	IStream* pstrm;

	AVIMAINHEADER avih;
	AVISTREAMHEADER strhAudio;
	WAVEFORMATEX strfAudio;
	AVISTREAMHEADER strhVideo;
	BITMAPINFOHEADER strfVideo;
	RGBQUAD dwColorTable[256];

	/* Audio buffers */
	DWORD dwBufferIndex;
	INT iCurrentBuffer;
	char* lpMainBuffer;
	WAVEHDR wvHdr1;
	WAVEHDR wvHdr2;
	HWAVEOUT hwo;

	CIRCULAR_BUFFER buffer;

	LPVOID lpBits;

	HWND hwnd;

} DCMPRSSCTX;

BYTE* decode_msv1_8bit(BYTE* stream, WORD wBlocksX, WORD wBlocksY, BYTE* lpBuffer, WORD wPitch) {
	INT x, y;
	BYTE* pixels;
	BYTE byte_a, byte_b;
	BYTE colors[8];
	WORD wBlocks = wBlocksX * wBlocksY;
	WORD wSkipBlocks = 0;
	WORD wFlags;

	for (y = 0; y < wBlocksY; y++) {
		pixels = lpBuffer;

		for (x = 0; x < wBlocksX; x++) {
			if (wBlocks == 0) goto done;

			if (wSkipBlocks) {
				wSkipBlocks--;
				wBlocks--;
				pixels += 4;
				continue;
			}

			byte_a = *(stream++);
			byte_b = *(stream++);

			if (wBlocks == 0) goto done;

			if (byte_b >= 0x84 && byte_b < 0x88) { /* Skip block */
				wSkipBlocks = ((byte_b - 0x84) << 8) + byte_a - 1;
			}
			else if (byte_b < 0x80) { /* 2-color encoding */
				wFlags = (byte_b << 8) | byte_a;
				colors[0] = *(stream++);
				colors[1] = *(stream++);

				wFlags = ~wFlags;

				pixels[0] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[1] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[2] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[3] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels += wPitch;

				pixels[0] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[1] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[2] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[3] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels += wPitch;

				pixels[0] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[1] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[2] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[3] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels += wPitch;

				pixels[0] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[1] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[2] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[3] = colors[wFlags & 1];

				pixels -= wPitch * 3;

			}
			else if (byte_b >= 0x90) { /* 8-color encoding */
				wFlags = (byte_b << 8) | byte_a; wFlags = ~wFlags;
				colors[0] = *(stream++);
				colors[1] = *(stream++);
				colors[2] = *(stream++);
				colors[3] = *(stream++);
				colors[4] = *(stream++);
				colors[5] = *(stream++);
				colors[6] = *(stream++);
				colors[7] = *(stream++);

				pixels[0] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[1] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[2] = colors[2 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[3] = colors[2 | (wFlags & 1)];
				wFlags >>= 1;
				pixels += wPitch;

				pixels[0] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[1] = colors[wFlags & 1];
				wFlags >>= 1;
				pixels[2] = colors[2 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[3] = colors[2 | (wFlags & 1)];
				wFlags >>= 1;
				pixels += wPitch;

				pixels[0] = colors[4 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[1] = colors[4 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[2] = colors[6 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[3] = colors[6 | (wFlags & 1)];
				wFlags >>= 1;
				pixels += wPitch;

				pixels[0] = colors[4 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[1] = colors[4 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[2] = colors[6 | (wFlags & 1)];
				wFlags >>= 1;
				pixels[3] = colors[6 | (wFlags & 1)];

				pixels -= wPitch * 3;

			}
			else { /* 1-color encoding */
				pixels[0] = byte_a;
				pixels[1] = byte_a;
				pixels[2] = byte_a;
				pixels[3] = byte_a;
				pixels += wPitch;
				pixels[0] = byte_a;
				pixels[1] = byte_a;
				pixels[2] = byte_a;
				pixels[3] = byte_a;
				pixels += wPitch;
				pixels[0] = byte_a;
				pixels[1] = byte_a;
				pixels[2] = byte_a;
				pixels[3] = byte_a;
				pixels += wPitch;
				pixels[0] = byte_a;
				pixels[1] = byte_a;
				pixels[2] = byte_a;
				pixels[3] = byte_a;

				pixels -= wPitch * 3;
			}

			pixels += 4;
		}

		lpBuffer += wPitch * 4;
	}

done:
	return stream;
}

void writeWaveHeader(HWAVEOUT hwo, WAVEHDR* pWaveHdr) {
	waveOutUnprepareHeader(hwo, pWaveHdr, sizeof(WAVEHDR));
	waveOutPrepareHeader(hwo, pWaveHdr, sizeof(WAVEHDR));
	waveOutWrite(hwo, pWaveHdr, sizeof(WAVEHDR));
}

void CALLBACK waveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	DCMPRSSCTX* pCtx = dwInstance;

	if (uMsg == WOM_DONE) {
		WAVEHDR* pWaveHdr;

		printf("Swapping buffer\n");

		if (pCtx->iCurrentBuffer == 0) {
			pWaveHdr = &(pCtx->wvHdr1);
		}
		else {
			pWaveHdr = &(pCtx->wvHdr2);
		}

		writeWaveHeader(hwo, pWaveHdr);
	}
}

void waveHdrInit(WAVEHDR* pWaveHdr, LPBYTE lpData, DWORD dwBufferLength, DWORD dwUser, DWORD dwFlags, DWORD dwLoops) {
	pWaveHdr->lpData = lpData;
	pWaveHdr->dwBufferLength = dwBufferLength;
	pWaveHdr->dwBytesRecorded = dwBufferLength;
	pWaveHdr->dwUser = dwUser;
	pWaveHdr->dwFlags = dwFlags;
	pWaveHdr->dwLoops = dwLoops;
}

int waveInit(DCMPRSSCTX* pCtx) {
	int res;
	DWORD dwBufferSize = pCtx->avih.dwInitialFrames * pCtx->strhAudio.dwSuggestedBufferSize * pCtx->strfAudio.wBitsPerSample / 8;

	res = waveOutOpen(&(pCtx->hwo), WAVE_MAPPER, &(pCtx->strfAudio), waveOutCallback, pCtx, CALLBACK_FUNCTION);

	pCtx->lpMainBuffer = malloc(dwBufferSize * 2);
	pCtx->dwBufferIndex = 0;

	/* Allocate and init the wave headers */
	waveHdrInit(&(pCtx->wvHdr1), pCtx->lpMainBuffer, dwBufferSize, 0, 0, 0);
	waveHdrInit(&(pCtx->wvHdr2), pCtx->lpMainBuffer + dwBufferSize, dwBufferSize, 0, 0, 0);
	pCtx->iCurrentBuffer = 0;

	return res;
}

void writeWaveData(DCMPRSSCTX* pCtx, LPBYTE lpData, DWORD dwSize) {
	DWORD dwBytesRemaining = 2 * pCtx->avih.dwInitialFrames * pCtx->strhAudio.dwSuggestedBufferSize * pCtx->strfAudio.wBitsPerSample / 8 - pCtx->dwBufferIndex;

	if (dwBytesRemaining < dwSize) {
		memcpy(pCtx->lpMainBuffer + pCtx->dwBufferIndex, lpData, dwBytesRemaining);
		memcpy(pCtx->lpMainBuffer, lpData + dwBytesRemaining, dwSize - dwBytesRemaining);
		pCtx->dwBufferIndex = dwSize - dwBytesRemaining;
	}
	else {
		memcpy(pCtx->lpMainBuffer + pCtx->dwBufferIndex, lpData, dwSize);
		pCtx->dwBufferIndex += dwSize;
	}
}

void startPlayback(DCMPRSSCTX* pCtx) {
	writeWaveHeader(pCtx->hwo, &(pCtx->wvHdr1));
	writeWaveHeader(pCtx->hwo, &(pCtx->wvHdr2));
}


const CHAR* szVideoWndClass = "VideoWndClass";

typedef struct _FileStream {
	IStream iface;
	FILE* fp;
	ULONG ulRc;
} FILESTREAM, * PFILESTREAM;

HRESULT __stdcall FileStream_QueryInterface(IStream* This, REFIID riid, void** ppvObject) {
	/* Check parameters */
	if (ppvObject == NULL) return E_POINTER;

	/* Check to make sure the interface is supported */
	if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ISequentialStream) || IsEqualIID(riid, &IID_IStream)) {
		*ppvObject = This;
		return S_OK;
	}

	*ppvObject = NULL;
	return E_NOINTERFACE;
}

ULONG __stdcall FileStream_AddRef(IStream* This) {
	PFILESTREAM pStrm = This;
	return InterlockedIncrement(&(pStrm->ulRc));
}

ULONG __stdcall FileStream_Release(IStream* This) {
	PFILESTREAM pStrm = This;
	ULONG ulRc = InterlockedDecrement(&(pStrm->ulRc));

	if (!ulRc) {
		fclose(pStrm->fp);
		free(pStrm);
	}

	return ulRc;
}

HRESULT __stdcall FileStream_Read(IStream* This, void* pv, ULONG cb, ULONG* pcbRead) {
	PFILESTREAM pStrm = (PFILESTREAM)This;

	/* Validate parameters */
	if (!pv) return STG_E_INVALIDPOINTER;

	/* Perform the read */
	*pcbRead = fread(pv, 1, cb, pStrm->fp);

	if (*pcbRead < cb) {
		if (ferror(pStrm->fp)) return STG_E_ACCESSDENIED; /* There was an error */
		return S_FALSE; /* No, we just reached the end of the file */
	}

	return S_OK;
}

HRESULT __stdcall FileStream_Seek(IStream* This, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) {
	PFILESTREAM pStrm = This;

	/* Perform the seek */
	if (fseek(pStrm->fp, dlibMove.LowPart, dwOrigin)) return STG_E_INVALIDFUNCTION;

	plibNewPosition->LowPart = ftell(pStrm->fp);
	return S_OK;
}

IStreamVtbl FileStreamVtbl = { .AddRef = &FileStream_AddRef, .Release = &FileStream_Release, .QueryInterface = &FileStream_QueryInterface,
							   .Read = &FileStream_Read, .Seek = &FileStream_Seek };

HRESULT OpenFileStream(FILE* fp, IStream** ppstm) {
	PFILESTREAM pStrm;

	/* Validate parameters */
	if (!fp) return STG_E_INVALIDPARAMETER;
	if (!ppstm) return STG_E_INVALIDPOINTER;

	/* Allocate interface */
	pStrm = *ppstm = malloc(sizeof(FILESTREAM));
	if (!(*ppstm)) return STG_E_INSUFFICIENTMEMORY;

	/* Initialize file stream */
	pStrm->fp = fp;
	pStrm->iface.lpVtbl = &FileStreamVtbl;
	pStrm->ulRc = 1;

	return S_OK;
}

#define StreamFromFourCC(fcc)		((fcc) & 0xFFFF)
#define TypeFromFourCC(fcc)			((fcc) >> 16)

/**
  *			Window messages are used to control the Video Window.
  *
  *			VM_LOAD: Load a video for playback
  *				lParam: Pointer to an IStream interface
  *				Returns 0 for success, error code for failure
  *
  *			VM_STOP: Stops playing the current video and unloads.
  *
  *			VM_PLAY:
  *
  *			VM_PAUSE:
  *
  *			VM_SEEK:
  */

#define VM_LOAD		WM_USER+0
#define VM_STOP		WM_USER+1
#define VM_PLAY		WM_USER+2
#define VM_PAUSE	WM_USER+3
#define VM_SEEK		WM_USER+4
#define VM_ATTACH	WM_USER+5
#define VM_INTERNAL	WM_USER+0x10
#define VMI_FRAME	VM_INTERNAL+0

#define VIDWND_LOAD(hwnd, lpStrm)			SendMessageA(hwnd, VM_LOAD, 0, lpStrm)
#define VIDWND_STOP(hwnd)					SendMessageA(hwnd, VM_STOP, 0, 0)
#define VIDWND_PLAY(hwnd)					SendMessageA(hwnd, VM_PLAY, 0, 0)
#define VIDWND_PAUSE(hwnd)					SendMessageA(hwnd, VM_PAUSE, 0, 0)
#define VIDWND_SEEK(hwnd, lOffset, wOrigin)	SendMessageA(hwnd, VM_SEEK, wOrigin, lOffset)
#define VIDWND_ATTACH(hwnd, hattach)		SendMessageA(hwnd, VM_ATTACH, 0, hattack)

#define GWLP_VIDWND_CTX		0




#define fccRIFF		MAKEFOURCC('R', 'I', 'F', 'F')
#define fccAVI		MAKEFOURCC('A', 'V', 'I', ' ')
#define fccLIST		MAKEFOURCC('L', 'I', 'S', 'T')
#define fccHDRL		MAKEFOURCC('h', 'd', 'r', 'l')
#define fccMOVI		MAKEFOURCC('m', 'o', 'v', 'i')

#define fccAVIH		MAKEFOURCC('a', 'v', 'i', 'h')
#define fccSTRL		MAKEFOURCC('s', 't', 'r', 'l')
#define fccSTRH		MAKEFOURCC('s', 't', 'r', 'h')
#define fccSTRF		MAKEFOURCC('s', 't', 'r', 'f')

#define fccVIDS		MAKEFOURCC('v', 'i', 'd', 's')
#define fccAUDS		MAKEFOURCC('a', 'u', 'd', 's')


#define FCC_CHARS(fcc)	(fcc), (fcc) >> 8, (fcc) >> 16, (fcc) >> 24

#define AVI_OK 0
#define AVIERR_IO -1
#define AVIERR_BADFMT -2
#define AVIERR_CORRUPT -3

typedef struct {
	DWORD dwFourCC;
	DWORD dwSize;
} CHUNK;

typedef struct {
	DWORD dwList;
	DWORD dwSize;
	DWORD dwFourCC;
} LIST;



void CALLBACK timerProc(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
	PostMessageA(dwUser, VMI_FRAME, 0, 0);
}



/* Read the entire list header */
HRESULT AVI_ReadList(IStream* pstrm, LIST* pList) {
	ULONG nread;
	return pstrm->lpVtbl->Read(pstrm, pList, sizeof(LIST), &nread);
}

/* Read the entire chunk header */
HRESULT AVI_ReadChunk(IStream* pstrm, CHUNK* pChunk) {
	ULONG nread;
	return pstrm->lpVtbl->Read(pstrm, pChunk, sizeof(CHUNK), &nread);
}

/* Read in a chunk's data with the chunk */
HRESULT AVI_ReadChunkData(IStream* pstrm, CHUNK* pChunk, PBYTE lpData) {
	ULONG nread;
	HRESULT hResult = pstrm->lpVtbl->Read(pstrm, lpData + sizeof(CHUNK), pChunk->dwSize, &nread);
	memcpy(lpData, pChunk, sizeof(CHUNK));
	if (nread & 1) pstrm->lpVtbl->Read(pstrm, &nread, 1, &nread);
	return hResult;
}

/* Read in a chunk's data without the chunk */
HRESULT AVI_ReadChunkDat(IStream* pstrm, CHUNK* pChunk, PBYTE lpData) {
	ULONG nread;
	HRESULT hResult = pstrm->lpVtbl->Read(pstrm, lpData, pChunk->dwSize, &nread);
	if (nread & 1) pstrm->lpVtbl->Read(pstrm, &nread, 1, &nread);
	return hResult;
}

/* Read in a list header after having already fetched a chunk */
HRESULT AVI_ReadListHeader(IStream* pstrm, CHUNK* pChunk, PBYTE pList) {
	ULONG nread;
	memcpy(pList, pChunk, sizeof(CHUNK));
	return pstrm->lpVtbl->Read(pstrm, pList + sizeof(CHUNK), sizeof(DWORD), &nread);
}

#define AVI_ERROR(err)		{ pstrm->lpVtbl->Release(pstrm); return err; }

/* Return -1 on failure, new offset otherwise */
int AVISeek(IStream* pstrm, DWORD dwOffset, DWORD dwOrigin) {
	LARGE_INTEGER dlibMove;
	ULARGE_INTEGER libNewPosition;

	dlibMove.QuadPart = dwOffset;

	if (pstrm->lpVtbl->Seek(pstrm, dlibMove, dwOrigin, &libNewPosition) != S_OK) return -1;

	return libNewPosition.LowPart;
}

#define AVITell(pstrm)		(AVISeek(pstrm, 0, STREAM_SEEK_CUR))

int AVI_ParseStreamList(IStream* pstrm, DCMPRSSCTX* lpContext, LIST* pHdr) {
	AVISTREAMHEADER avsh;

	DWORD offs_start = AVITell(pstrm);
	CHUNK ck;
	LIST list;
	BOOL bReadStreamHeader = 0;
	BOOL bReadStreamFormat = 0;

	while (AVITell(pstrm) < (offs_start + pHdr->dwSize - 4)) {
		if (AVI_ReadChunk(pstrm, &ck) != S_OK) AVI_ERROR(AVIERR_IO);

		switch (ck.dwFourCC) {
		case fccSTRH:
			if (bReadStreamHeader) AVI_ERROR(AVIERR_BADFMT);
			if (AVI_ReadChunkData(pstrm, &ck, &avsh) != S_OK) AVI_ERROR(AVIERR_IO);
			bReadStreamHeader = 1;
			break;
		case fccSTRF:
			if (!bReadStreamHeader || bReadStreamFormat) AVI_ERROR(AVIERR_BADFMT);
			if (avsh.fccType == fccVIDS) {
				lpContext->strhVideo = avsh;
				if (AVI_ReadChunkDat(pstrm, &ck, &(lpContext->strfVideo)) != S_OK) AVI_ERROR(AVIERR_IO);
			}
			else if (avsh.fccType == fccAUDS) {
				lpContext->strhAudio = avsh;
				if (AVI_ReadChunkDat(pstrm, &ck, &(lpContext->strfAudio)) != S_OK) AVI_ERROR(AVIERR_IO);
			}
			bReadStreamFormat = 1;
			break;
		default:
			break;
		}
	}

	return AVI_OK;
}

int AVIParseHeaders(IStream* pstrm, DCMPRSSCTX* lpContext, LIST* pHdr) {
	DWORD offs_start = AVITell(pstrm);
	CHUNK ck;
	LIST list;
	BOOL bReadMainHeader = 0;

	while (AVITell(pstrm) < (offs_start + pHdr->dwSize - 4)) {
		if (AVI_ReadChunk(pstrm, &ck) != S_OK) AVI_ERROR(AVIERR_IO);

		if (ck.dwFourCC == fccAVIH && !bReadMainHeader) {
			if (AVI_ReadChunkData(pstrm, &ck, &(lpContext->avih)) != S_OK) AVI_ERROR(AVIERR_IO);
			/* This contains the number of streams... */

			lpContext->buffer.iRead = 0;
			lpContext->buffer.iWritten = 0;
			lpContext->buffer.iSize = lpContext->avih.dwInitialFrames;
			lpContext->buffer.dwRecSize = lpContext->avih.dwSuggestedBufferSize;
			lpContext->buffer.lpBuffer = malloc(lpContext->buffer.dwRecSize * lpContext->buffer.iSize);

			if (!lpContext) AVI_ERROR(AVIERR_IO);

			bReadMainHeader = 1;

		}
		else if (ck.dwFourCC == fccLIST && bReadMainHeader) {
			int err;
			if (AVI_ReadListHeader(pstrm, &ck, &list) != S_OK) AVI_ERROR(AVIERR_IO);

			if (list.dwFourCC != fccSTRL) AVI_ERROR(AVIERR_BADFMT);

			err = AVI_ParseStreamList(pstrm, lpContext, &list);
			if (err) AVI_ERROR(err);
			printf("parsed stram list\n");
		}
		else {
			/*printf("%c%c%c%c\n", FCC_CHARS(ck.dwFourCC));
			AVI_ERROR(AVIERR_BADFMT);*/
		}
	}

	/* Seek to send of HDRL? */

	return AVI_OK;
}

void processRec(DCMPRSSCTX* lpContext, LPBYTE lpRec) {
	DWORD dwStreamPtr = 12;
	LIST* pList = lpRec;

	while (dwStreamPtr < (pList->dwSize - 8)) {
		CHUNK* pChunk = lpRec + dwStreamPtr;
		LPBYTE lpData = pChunk + 1;

		//printf("%c%c%c%c\n", FCC_CHARS(pChunk->dwFourCC));

		if (TypeFromFourCC(pChunk->dwFourCC) == TypeFromFourCC(MAKEFOURCC('0', '0', 'd', 'c'))) {
			decode_msv1_8bit(lpData, 39, 30, lpContext->lpBits, 156);
		}
		else if (TypeFromFourCC(pChunk->dwFourCC) == TypeFromFourCC(MAKEFOURCC('0', '0', 'w', 'b'))) {
			printf("Audio data?\n");
			writeWaveData(lpContext, lpData, pChunk->dwSize);
		}

		dwStreamPtr += sizeof(CHUNK) + pChunk->dwSize;

		if (dwStreamPtr & 1) dwStreamPtr++;
	}
}

int processAndReadRec(IStream* pstrm, DCMPRSSCTX* lpContext) {
	CHUNK ck;

	processRec(lpContext, lpContext->buffer.lpBuffer + lpContext->buffer.dwRecSize * (lpContext->buffer.iRead % lpContext->buffer.iSize));
	lpContext->buffer.iRead++;

	if (AVI_ReadChunk(pstrm, &ck) != S_OK) AVI_ERROR(AVIERR_IO);
	if (AVI_ReadChunkData(pstrm, &ck, lpContext->buffer.lpBuffer + lpContext->buffer.dwRecSize * (lpContext->buffer.iWritten % lpContext->buffer.iSize)) != S_OK) AVI_ERROR(AVIERR_IO);
	lpContext->buffer.iWritten++;
}

#define FCC_CHARS(fcc)	(fcc), (fcc) >> 8, (fcc) >> 16, (fcc) >> 24



int AVIPreload(IStream* pstrm, DCMPRSSCTX* lpContext, LIST* pHdr) {
	CHUNK ck;

	/* Read the initial audio buffer */
	DWORD dwInitialFrames = lpContext->avih.dwInitialFrames;

	//printf("%d\n", AVITell(pstrm));

	while (dwInitialFrames--) {
		if (AVI_ReadChunk(pstrm, &ck) != S_OK) AVI_ERROR(AVIERR_IO);
		//printf("%c%c%c%c\n", FCC_CHARS(ck.dwFourCC));
		if (AVI_ReadChunkData(pstrm, &ck, lpContext->buffer.lpBuffer) != S_OK) AVI_ERROR(AVIERR_IO);
		processRec(lpContext, lpContext->buffer.lpBuffer);
	}

	/* Start reading in records */
	dwInitialFrames = lpContext->buffer.iSize;

	while (dwInitialFrames--) {
		LPBYTE lpDest = lpContext->buffer.lpBuffer + lpContext->buffer.dwRecSize * (lpContext->buffer.iWritten % lpContext->buffer.iSize);
		if (AVI_ReadChunk(pstrm, &ck) != S_OK) AVI_ERROR(AVIERR_IO);
		if (AVI_ReadChunkData(pstrm, &ck, lpDest) != S_OK) AVI_ERROR(AVIERR_IO);
		lpContext->buffer.iWritten++;
	}

	lpContext->strfVideo.biCompression = BI_RGB;
	lpContext->lpBits = malloc(156 * 120);

	startPlayback(lpContext);
	timeBeginPeriod(1);
	timeSetEvent(66, 0, timerProc, lpContext->hwnd, TIME_PERIODIC);
	return AVI_OK;
}

/* Parse the beginning of an AVI file, reading in the header list and doing preload from MOVI. Return 0 on success */
int AVIParse(IStream* pstrm, void* lpContext, LIST* pHdr) {
	DWORD offs_start = AVITell(pstrm);
	CHUNK ck;
	LIST list;
	DWORD state = 0; /* 0 = expecting HDRL, 1 = expecting MOVI */

	while (AVITell(pstrm) < (offs_start + pHdr->dwSize - 4)) {
		if (AVI_ReadChunk(pstrm, &ck) != S_OK) AVI_ERROR(AVIERR_IO);

		if (ck.dwFourCC == fccLIST) {
			if (AVI_ReadListHeader(pstrm, &ck, &list) != S_OK) AVI_ERROR(AVIERR_IO);

			if (list.dwFourCC == fccHDRL && state == 0) {
				int res = AVIParseHeaders(pstrm, lpContext, &list);
				if (res) AVI_ERROR(res);
				state = 1;
			}
			else if (list.dwFourCC == fccMOVI && state == 1) {
				waveInit(lpContext);
				return AVIPreload(pstrm, lpContext, &list);
			}
			else AVI_ERROR(AVIERR_BADFMT);
		}
		else AVI_ERROR(AVIERR_BADFMT);
	}

	return AVI_OK;
}


int AVILoad(IStream* pstrm, void* lpContext) {
	/* Read in the RIFF header */
	LIST header;
	if (AVI_ReadList(pstrm, &header) != S_OK) AVI_ERROR(AVIERR_IO);
	if (header.dwList != fccRIFF || header.dwFourCC != fccAVI) AVI_ERROR(AVIERR_BADFMT);

	/* Validate file size */

	return AVIParse(pstrm, lpContext, &header);
}



LRESULT CALLBACK VideoWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	DCMPRSSCTX* lpContext = GetWindowLongPtrA(hWnd, GWLP_VIDWND_CTX);

	switch (Msg) {
	case WM_CREATE:
		lpContext = malloc(sizeof(DCMPRSSCTX));
		memset(lpContext, 0, sizeof(DCMPRSSCTX));
		SetWindowLongPtrA(hWnd, GWLP_VIDWND_CTX, lpContext);
		lpContext->hwnd = hWnd;
		break;

	case VM_LOAD: {
		/* Get access to the stream */
		IStream* lpStrm = (IStream*)lParam;
		lpStrm->lpVtbl->AddRef(lpStrm);
		lpContext->pstrm = lpStrm;
		return AVILoad(lpStrm, lpContext);
	}

	case VMI_FRAME: {
		processAndReadRec(lpContext->pstrm, lpContext);
		InvalidateRect(hWnd, NULL, 0);
		break;
	}

	case WM_PAINT: {
		RECT clientRect;
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);

		GetClientRect(hWnd, &clientRect)
			;
		if (lpContext->lpBits) {
			StretchDIBits(hDC, 0, 0, clientRect.right, clientRect.bottom, 0, 0, 156, 120, lpContext->lpBits, &(lpContext->strfVideo), DIB_RGB_COLORS, SRCCOPY);
		}

		EndPaint(hWnd, &ps);
	}
	}

	return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

BOOL VideoWndRegisterClass() {
	WNDCLASSA wc = { 0 };

	wc.lpfnWndProc = VideoWndProc;
	wc.cbWndExtra = 4;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = szVideoWndClass;

	return RegisterClassA(&wc);
}

int main() {
	MSG msg = { 0 };
	HWND hwnd;

	IStream* pStrm;

	FILE* fp = fopen("wndsurf1.avi", "rb");

	OpenFileStream(fp, &pStrm);

	printf("%p\n", VideoWndRegisterClass());

	hwnd = CreateWindowA(szVideoWndClass, "Video Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, GetModuleHandle(NULL), 0);
	ShowWindow(hwnd, SW_SHOW);

	printf("%d\n", VIDWND_LOAD(hwnd, pStrm));

	while (GetMessageA(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return 0;
}