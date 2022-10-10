#ifndef HUD_NIGHTVISION_H
#define HUD_NIGHTVISION_H
#include "base.h"

class CHudNightvision : public CHudElemBase<CHudNightvision>
{
public:
	void Init( void );
	void VidInit( void );
	void Draw( float flTime );
	void Reset( void );
	int MsgFunc_Nightvision( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_Flashlight( const char *pszName, int iSize, void *pbuf );

private:
	HSPRITE m_hSprite1, m_hSprite2, m_hSprite3, m_hSprite4;

	int m_fOn;
	int m_iFrame, m_nFrameCount;
};
#endif