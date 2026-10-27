#ifndef __SCAN_LIMITER_H__
#define __SCAN_LIMITER_H__
#pragma ONCE
class CScanLimiter
{
	int nScannedUnits;
public:
	CScanLimiter() : nScannedUnits( 0 ) { }

	void SegmentsFinished();
	void TargetScanning( const EUnitRPGType &scanUnitType );

	bool CanScan() const;
	const int GetNScannedUnits() const { return nScannedUnits; }
	// The count runs from the middle of one segment to the middle of the next,
	// so a save between two segments holds it (CGlobalSerializer).
	void SetNScannedUnits( const int n ) { nScannedUnits = n; }
};
#endif //__SCAN_LIMITER_H__
