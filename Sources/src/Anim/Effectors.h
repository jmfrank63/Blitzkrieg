#ifndef __EFFECTORS_H__
#define __EFFECTORS_H__
template <class TEffector>
struct SEffector
{
	CPtr<TEffector> pEffector;						// effector itself
	int nID;															// effector ID
	SEffector() : nID( -1 ) {  }
	SEffector( TEffector *_pEffector, int _nID ) : pEffector( _pEffector ), nID( _nID ) {  }
	const SHMatrix& GetMatrix() const { return pEffector->GetMatrix(); }
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &pEffector );
		saver.Add( 2, &nID );
		return 0;
	}
};
typedef SEffector<IMatrixEffector> SMatrixEffectorDesc;
typedef std::list<SMatrixEffectorDesc> SMatrixEffectorsList;
#endif // __EFFECTORS_H__