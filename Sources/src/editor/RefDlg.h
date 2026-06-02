#ifndef __REF_DLG_H__
#define __REF_DLG_H__

#include "resource.h"

using std::string;


enum EReferenceType
{
	E_ANIMATIONS_REF, // sprites
	E_FUNC_PARTICLES_REF, //particles
	E_EFFECTS_REF, //effects
	E_WEAPONS_REF, //weapons
	E_SOLDIER_REF, //infantry
	E_ACTIONS_REF, //actions&exposures - MultiSelDialog
	E_SCENARIO_MISSIONS_REF, //missions
	E_TEMPLATE_MISSIONS_REF, //templates
	E_CHAPTERS_REF, //chapters
	E_SOUNDS_REF, //sounds
	E_SETTING_REF, //settings
	E_ASKS_REF, //asks
	E_CRATER_REF, //craters
	E_DEATHHOLE_REF, //deathholes
	E_MAP_REF, //maps
	E_MUSIC_REF, //maps
	E_MOVIE_REF, //movies
	E_PARTICLE_TEXTURE_REF, // particle textures
	E_ROAD_TEXTURE_REF, //road textures
	E_WATER_TEXTURE_REF, //water textures
};

class CReferenceDialog : public CDialog
{
public:
	CReferenceDialog(CWnd* pParent = NULL);   // standard constructor
	void Init( int nRefId );
	string GetValue();
	static void InitLists();

	enum { IDD = IDD_REFERENCE_DIALOG };
	CListBox m_refList;
	CString m_refVal;


	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	
protected:
	EReferenceType nReferenceType;
	static const std::string szSpritesDir;
	static const std::string szParticlesDir;
	static const std::string szEffectsDir;
	static const std::string szWeaponsDir;
	static const std::string szInfantryDir;
	static const std::string szMissionDir;
	static const std::string szMissionDir1;
	static const std::string szMissionDir2;
	static const std::string szTemplateDir;
	static const std::string szChapterDir;
	static const std::string szChapterDir1;
	static const std::string szSettingDir;
	static const std::string szAskDir;
	static const std::string szDeathDir;
	static const std::string szCraterDir;
	static const std::string szMapDir;
	static const std::string szMusicDir;
	static const std::string szMovieDir;
	static const std::string szParticleTextureDir;
	static const std::string szRoadTextureDir;
	static const std::string szWaterTextureDir;
	static std::list<std::string> spritesList;
	static std::list<std::string> particlesList;
	static std::list<std::string> effectsList;
	static std::list<std::string> weaponsList;
	static std::list<std::string> soldiersList;
	static std::list<std::string> scenariomissionsList;
	static std::list<std::string> templatemissionsList;
	static std::list<std::string> chaptersList;
	static std::list<std::string> soundsList;
	static std::list<std::string> settingList;
	static std::list<std::string> asksList;
	static std::list<std::string> craterList;
	static std::list<std::string> deathholeList;
	static std::list<std::string> mapList;
	static std::list<std::string> musicList;
	static std::list<std::string> movieList;
	static std::list<std::string> particleTextureList;
	static std::list<std::string> roadTextureList;
	static std::list<std::string> waterTextureList;

protected:
	static bool CheckParentDir( const std::string &szDir, const std::string &szPath );
	static bool CheckExtention( const std::string &szExt, const std::string &szPath );
	static std::string StripFilename( const std::string &szDir, const std::string &szExt, const std::string &szPath );
	void FillFromList( const std::list<std::string> &entries );
	void LoadItems( EReferenceType eType );
	void CheckedAdd( const std::list<std::string> &entries, const std::string &szRef );
		
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkReferenceList();
	DECLARE_MESSAGE_MAP()
};

#endif		//__REF_DLG_H__
