/*
 *	File: SLIPSTRM.C
 *
 *	SLIPSTREAM 3D rendering engine for 
 *  HYPERSPACE game engine.
 *
 *	Copyright (c) 2025 by Will Klees
 *
 *	Changelog
 *	01/22/25: Created
 *  01/25/25: Z-sorts polygons
 */

#include <SLIPSTRM/SLIPSTRM.H>
#include <SURFACE.H>
#include <stdlib.h>

VEC3 VecBuffer[256];
GFX_INTERNAL_TRI TriBuffer[POLY_BUF_SIZE];
int VecBufIdx = 0;
int PolyBufIdx = 0;

/* Compare/Zsort two triangles */
int cmp_tri(GFX_INTERNAL_TRI* tri1, GFX_INTERNAL_TRI* tri2) {
    return ((tri1->z < tri2->z) - (tri1->z > tri2->z));
}

uint32_t polygons_rendered;

/* Draws a 3D mesh */
void gfxDrawIndexed(GFX_TRIANGLE* pIndexBuffer, uint16_t nIndices, VEC3* pVertexList, uint16_t nVertices, int drawMode, uint8_t color_override) {
    int i;
    VEC3 v;

    VecBufIdx = 0;
    PolyBufIdx = 0;

    for (i = 0; i < nVertices; i++) {
        gfx_Transform(&pVertexList[i], &v);
        gfx_Project(&v, &VecBuffer[VecBufIdx++]);
    }    

    for (i = 0; i < nIndices; i++) {
        GFX_TRIANGLE* pTri = &pIndexBuffer[i];

        FIXED z1 = VecBuffer[pTri->iVert1].z;
        FIXED z2 = VecBuffer[pTri->iVert2].z;
        FIXED z3 = VecBuffer[pTri->iVert3].z;

        if (z1 <= 0 || z2 <= 0 || z3 <= 0) continue;

        TriBuffer[PolyBufIdx].v1 = &VecBuffer[pTri->iVert1];
        TriBuffer[PolyBufIdx].v2 = &VecBuffer[pTri->iVert2];
        TriBuffer[PolyBufIdx].v3 = &VecBuffer[pTri->iVert3];
        TriBuffer[PolyBufIdx].z = z1 + z2 + z3;
        TriBuffer[PolyBufIdx].cColor = color_override ? color_override : pTri->cColor;

        PolyBufIdx++;
    }

    if (drawMode == GFX_FILL) {
        qsort(TriBuffer, PolyBufIdx, sizeof(GFX_INTERNAL_TRI), (sorter)cmp_tri);
    }

    for (i = 0; i < PolyBufIdx; i++) {
        GFX_INTERNAL_TRI *pTri = &TriBuffer[i];
        char color = pTri->cColor;
        polygons_rendered++;

        if (drawMode == GFX_FILL) {
            Surface.FillTri(pTri->v1->x, pTri->v1->y, 
                            pTri->v2->x, pTri->v2->y, 
                            pTri->v3->x, pTri->v3->y, color << 4 | 15);
        } else {
            Surface.DrawLine(pTri->v1->x, pTri->v1->y, pTri->v2->x, pTri->v2->y, pTri->cColor);
            Surface.DrawLine(pTri->v1->x, pTri->v1->y, pTri->v3->x, pTri->v3->y, pTri->cColor);
            Surface.DrawLine(pTri->v3->x, pTri->v3->y, pTri->v2->x, pTri->v2->y, pTri->cColor);
        }
    }
}

typedef struct _Z_BIN_ENTRY {
    struct _Z_BIN_ENTRY* next;
    GFX_TRIANGLE tri;
} Z_BIN_ENTRY;

typedef struct _Z_BIN {
    Z_BIN_ENTRY* start;
} Z_BIN;

#define MAX_VERTS 1000
#define MAX_TRIS 1000

int GFX_VERTEX_INDEX = 0;
int GFX_POLYGON_INDEX = 0;

VEC3 GFX_VERTEX_BUFFER[MAX_VERTS];
VEC3 GFX_ORIG_VERTEX_BUFFER[MAX_VERTS];
Z_BIN_ENTRY GFX_TRIANGLE_BUFFER[MAX_TRIS];
Z_BIN GFX_Z_BIN[32768];

void gfxVertex(VEC3* v, MAT4 ModelMatrix) {
    VEC3 temp;

    //GFX_ORIG_VERTEX_BUFFER[GFX_VERTEX_INDEX] = *v;
    MultM4xV3(ModelMatrix, v, &(GFX_ORIG_VERTEX_BUFFER[GFX_VERTEX_INDEX]));

    gfx_Transform(v, &temp);
    gfx_Project(&temp, &(GFX_VERTEX_BUFFER[GFX_VERTEX_INDEX++]));
}

void gfxAddToBin(int bin, Z_BIN_ENTRY* entry) {
    entry->next = GFX_Z_BIN[bin].start;
    GFX_Z_BIN[bin].start = entry;
}

void gfxAddTri(int bin, uint16_t v1, uint16_t v2, uint16_t v3, uint8_t color) {
    GFX_TRIANGLE_BUFFER[GFX_POLYGON_INDEX].tri.iVert1 = v1;
    GFX_TRIANGLE_BUFFER[GFX_POLYGON_INDEX].tri.iVert2 = v2;
    GFX_TRIANGLE_BUFFER[GFX_POLYGON_INDEX].tri.iVert3 = v3;
    GFX_TRIANGLE_BUFFER[GFX_POLYGON_INDEX].tri.cColor = color;
    gfxAddToBin(bin, &(GFX_TRIANGLE_BUFFER[GFX_POLYGON_INDEX++]));
}

void gfxSubmitTri(uint16_t v1, uint16_t v2, uint16_t v3, uint8_t color) {
    FIXED z = GFX_VERTEX_BUFFER[v1].z / 3 + GFX_VERTEX_BUFFER[v2].z / 3 + GFX_VERTEX_BUFFER[v3].z / 3;

    if (GFX_VERTEX_BUFFER[v1].z > nearClip && GFX_VERTEX_BUFFER[v2].z > nearClip && GFX_VERTEX_BUFFER[v3].z > nearClip) {
        gfxAddTri(FIXTOI(z), v1, v2, v3, color);
    }
}

#include <math.h>

void vec3_normalize(VEC3* v) {
    float x = FIXTOF(v->x);
    float y = FIXTOF(v->y);
    float z = FIXTOF(v->z);
    float mag = sqrt(x * x + y * y + z * z);

    v->x /= mag;
    v->y /= mag;
    v->z /= mag;
}

FIXED find_illum(VEC3* x1, VEC3* x2, VEC3* x3, VEC3* light) {
    VEC3 look;
	VEC3 norm;
	VEC3 v1 = *x2;
	VEC3 v2 = *x3;
	vec3_sub(x1, &v1);
	vec3_sub(x1, &v2);
	CrossV3xV3(&v1, &v2, &norm);
	vec3_normalize(&norm);
	look = *x1;
	vec3_add(x2, &look);
	vec3_add(x3, &look);
	look.x /= 3;
	look.y /= 3;
	look.z /= 3;
	vec3_sub(light, &look);
	vec3_normalize(&look);
	return abs(DotV3xV3(&look, &norm));
}

extern VEC3 cam_pos;

void gfxDrawTri(GFX_TRIANGLE* tri) {
    FIXED illum = (3*find_illum(&(GFX_ORIG_VERTEX_BUFFER[tri->iVert1]), &(GFX_ORIG_VERTEX_BUFFER[tri->iVert2]), &(GFX_ORIG_VERTEX_BUFFER[tri->iVert3]), &(cam_pos)) + ITOFIX(1)) >> 14;
    char color = tri->cColor << 4 | (illum&15);
    VEC3* v1 = &(GFX_VERTEX_BUFFER[tri->iVert1]);
    VEC3* v2 = &(GFX_VERTEX_BUFFER[tri->iVert2]);
    VEC3* v3 = &(GFX_VERTEX_BUFFER[tri->iVert3]);

    Surface.FillTri(v1->x, v1->y, v2->x, v2->y, v3->x, v3->y, color);
}

void gfxClear() {
    GFX_VERTEX_INDEX = 0;
    GFX_POLYGON_INDEX = 0;

    memset(GFX_Z_BIN, 0, sizeof(GFX_Z_BIN));
}

void gfxPresent() {
    int i;
    
    /* Draw each Z bin from back-to-front */
    for (i = 32767; i >= 0; i--) {
        Z_BIN_ENTRY* z_bin_entry = GFX_Z_BIN[i].start;

        while (z_bin_entry) {
            gfxDrawTri(&(z_bin_entry->tri));

            z_bin_entry = z_bin_entry->next;
        }
    }
}

