#ifndef _UIElement_h_included_
#define _UIElement_h_included_

class CUIButton : public IUIElement
{
};
class CUIButton : public IUIButton, public CUIButton
{
	DEFINE_UIELEMENT_BRIDGE;
	DECLARE_SUPER( CUIButton );

};
#endif//_UIElement_h_included_