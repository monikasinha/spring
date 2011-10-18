/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

//
// ROAM Simplistic Implementation
// Added to Spring by Peter Sarkozy (mysterme AT gmail DOT com)
// Billion thanks to Bryan Turner (Jan, 2000)
//                    brturn@bellsouth.net
//
// Based on the Tread Marks engine by Longbow Digital Arts
//                               (www.LongbowDigitalArts.com)
// Much help and hints provided by Seumas McNally, LDA.


#include "Landscape.h"
#include "Game/Camera.h"
#include "Map/MapDamage.h"
#include "Map/SMF/SMFGroundDrawer.h"
#include "Rendering/GL/VertexArray.h"
#include "Sim/Misc/GlobalConstants.h"
#include "System/LogOutput.h"


// -------------------------------------------------------------------------------------------------
//	LANDSCAPE CLASS
// -------------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------
// Definition of the static member variables
//
int Landscape::m_NextTriNode;
TriTreeNode Landscape::m_TriPool[POOL_SIZE];

// ---------------------------------------------------------------------
// Allocate a TriTreeNode from the pool.
//
TriTreeNode *Landscape::AllocateTri()
{
	TriTreeNode *pTri=NULL;

	// IF we've run out of TriTreeNodes, just return NULL (this is handled gracefully)
	if (m_NextTriNode >= POOL_SIZE)
		return NULL;

	pTri = &(m_TriPool[m_NextTriNode++]);
	pTri->LeftChild = pTri->RightChild = NULL;
	return pTri;
}

// ---------------------------------------------------------------------
// Initialize all patches
//
void Landscape::Init(const float *hMap, int bx, int by)//513 513
{
	heightData = hMap;

	// Store the Height Field array
	m_HeightMap = hMap;
	h = by;
	w = bx;
	updateCount = 0;

	m_Patches.resize((bx / PATCH_SIZE) * (by / PATCH_SIZE));
	minhpatch.resize((bx / PATCH_SIZE) * (by / PATCH_SIZE), -10000.0f);
	maxhpatch.resize((bx / PATCH_SIZE) * (by / PATCH_SIZE), 10000.0f);

	// Initialize all terrain patches
	for (int Y = 0; Y < by / PATCH_SIZE; Y++) {
		for (int X = 0; X < bx / PATCH_SIZE; X++) {
			Patch* patch = &(m_Patches[Y * (bx / PATCH_SIZE) + X]);
			for (int i = 0; i < PATCH_SIZE; i++) {
				for (int j = 0; j < PATCH_SIZE; j++) {
					maxhpatch[Y * bx / PATCH_SIZE + X]
							=MAX(maxhpatch[Y*bx/PATCH_SIZE+X], heightData[X*PATCH_SIZE+i+(Y*PATCH_SIZE+j)*(bx+1)]);
					minhpatch[Y * bx / PATCH_SIZE + X]
							=MIN(minhpatch[Y*bx/PATCH_SIZE+X], heightData[X*PATCH_SIZE+i+(Y*PATCH_SIZE+j)*(bx+1)]);
				}
			}
			patch->heightData=heightData;
			patch->Init(
					X * PATCH_SIZE,
					Y * PATCH_SIZE,
					hMap,
					bx,
					maxhpatch[Y * bx / PATCH_SIZE + X],
					minhpatch[Y * bx / PATCH_SIZE + X]);
			patch->ComputeVariance();
		}
	}
}

// ---------------------------------------------------------------------
// Reset all patches, recompute variance if needed
//
void Landscape::Reset()
{
	// Set the next free triangle pointer back to the beginning
	SetNextTriNode(0);

	// Go through the patches performing resets, compute variances, and linking.
	for (int Y = 0; Y < h / PATCH_SIZE; Y++)
		for (int X = 0; X < w / PATCH_SIZE; X++) {
			Patch* patch = &(m_Patches[Y * (w / PATCH_SIZE) + X]);

			// Reset the patch
			patch->Reset();
			patch->SetVisibility();

			// Check to see if this patch has been deformed since last frame.
			// If so, recompute the varience tree for it.
			//if (!mapDamage->disabled):

			//patch->ComputeVariance();

			//if (patch->isVisibile()) {
			if (true){
				// Link all the patches together.
				if (X > 0)
					patch->GetBaseLeft()->LeftNeighbor = m_Patches[Y * (w
							/ PATCH_SIZE) + X - 1].GetBaseRight();
				else
					patch->GetBaseLeft()->LeftNeighbor = NULL; // Link to bordering Landscape here..

				if (X < ((w / PATCH_SIZE) - 1))
					patch->GetBaseRight()->LeftNeighbor = m_Patches[Y * (w
							/ PATCH_SIZE) + X + 1].GetBaseLeft();
				else
					patch->GetBaseRight()->LeftNeighbor = NULL; // Link to bordering Landscape here..

				if (Y > 0)
					patch->GetBaseLeft()->RightNeighbor = m_Patches[(Y - 1)
							* (w / PATCH_SIZE) + X].GetBaseRight();
				else
					patch->GetBaseLeft()->RightNeighbor = NULL; // Link to bordering Landscape here..

				if (Y < ((h / PATCH_SIZE) - 1))
					patch->GetBaseRight()->RightNeighbor = m_Patches[(Y + 1)
							* (w / PATCH_SIZE) + X].GetBaseLeft();
				else
					patch->GetBaseRight()->RightNeighbor = NULL; // Link to bordering Landscape here..
			}
		}

}

// ---------------------------------------------------------------------
// Create an approximate mesh of the landscape.
//
void Landscape::Tessellate(float cx, float cy, float cz, int viewradius)
{
	// Perform Tessellation
	for (std::vector<Patch>::iterator it = m_Patches.begin(); it != m_Patches.end(); it++) {
		if (it->isVisibile())
			it->Tessellate(cx, cy, cz, viewradius);
	}
	updateCount = 0;
}

// ---------------------------------------------------------------------
// Render each patch of the landscape & adjust the frame variance.
//
int Landscape::Render(CSMFGroundDrawer* parent, bool camMoved, bool inShadowPass, bool waterdrawn)
{
	bool dirty = false;

	for (std::vector<Patch>::iterator it = m_Patches.begin(); it != m_Patches.end(); it++) {
		if (it->isDirty() == 1) {
			dirty = true;
			it->ComputeVariance();
		}
	}

	if (dirty) {
		Reset();
		Tessellate(0,0,0,200);
	}

	int nCount;
	int tricount = 0;
	Patch* patch = &(m_Patches[0]);

	for (nCount = 0; nCount < (w / PATCH_SIZE) * (h / PATCH_SIZE); nCount++, patch++) {
		if (patch->isVisibile()) {
			if (camMoved)
				patch->Render(parent, nCount, waterdrawn);

			if (!inShadowPass) {
				parent->SetupBigSquare(nCount % (w / PATCH_SIZE), nCount / (w / PATCH_SIZE));
			}
			
			tricount += patch->GetTriCount();
			patch->DrawTriArray(parent);
		}
	}

	//if (GetNextTriNode() != gDesiredTris)
	//	gFrameVariance += ((float) GetNextTriNode() - (float) gDesiredTris)
	//			/ (float) gDesiredTris;
	//if ( gFrameVariance < 0.5 )
	//gFrameVariance = 1;
	return tricount;
}

void Landscape::Explosion(float x, float y, float z, float radius)
{
	updateCount = 0;

	for (std::vector<Patch>::iterator it = m_Patches.begin(); it != m_Patches.end(); it++) {
		if (
			it->m_WorldX * SQUARE_SIZE                  < x + radius &&
			(it->m_WorldX + PATCH_SIZE) * SQUARE_SIZE   > x - radius &&
			it->m_WorldY * SQUARE_SIZE                  < z + radius &&
			(it->m_WorldY + PATCH_SIZE) * SQUARE_SIZE   > z - radius
		) {
			it->m_VarianceDirty = 1;
			updateCount++;
		}
	}
	//LogObject() << "Explosion: updating " << updateCount <<" patches";
	
}
