/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/

#include "nui.h"
#include "nuiLabel.h"
#include "nuiMainWindow.h"
#include "nuiTopLevel.h"
#include "nuiMetaPainter.h"
#include "nuiNotification.h"
#include "nuiCSS.h"

#define PARTIAL_REDRAW_DEFAULT false

#if 0//defined(_MULTI_TOUCHES_) && defined(_DEBUG_)
# define NGL_TOUCHES_DEBUG(X) (X)
# define _NGL_DEBUG_TOUCHES_
#else//!_MULTI_TOUCHES_
# define NGL_TOUCHES_DEBUG(X)
#endif//!_MULTI_TOUCHES_

#ifdef _NGL_DEBUG_TOUCHES_
#define PRINT_GRAB_IDS()\
{\
  for (nuiGrabMap::const_iterator it = mpGrab.begin(); it != mpGrab.end(); ++it)\
  {\
    NGL_OUT(_T("[%d] -> "), it->first);\
    if (!(it->second)) {\
      NGL_OUT(_T("NULL\n"));\
    } else {\
      NGL_OUT(_T("%ls\n"),\
                      it->second->GetProperty(_T("Class")).GetChars());\
    }\
  }\
}
#else
#define PRINT_GRAB_IDS() {}
#endif


class nuiToolTip : public nuiSimpleContainer
{
public:
  nuiToolTip();
  virtual ~nuiToolTip();

  virtual bool SetRect(const nuiRect& rRect);
  virtual nuiRect CalcIdealSize();

  virtual bool Draw(nuiDrawContext* pContext);

  void SetText(const nglString& rText);
  nglString GetText() const;

  virtual void BroadcastInvalidateLayout(nuiWidgetPtr pSender, bool BroadCastOnly)
  {
    Invalidate();
    mNeedSelfLayout = true;
    mNeedIdealRect = true;
    mNeedRender = true;
    DebugRefreshInfo();
  }

protected:
  nuiLabel* mpLabel;
};

nuiToolTip::nuiToolTip()
 : nuiSimpleContainer()
{
  SetObjectClass(_T("nuiToolTip"));
  SetObjectName(_T("ToolTipContainer"));
  mpLabel = new nuiLabel(nglString::Empty);
  mpLabel->SetWrapping(true);
  mpLabel->SetObjectName(_T("ToolTipLabel"));
  AddChild(mpLabel);
}

nuiToolTip::~nuiToolTip()
{
}

#define NUI_TOOLTIP_MARGIN_X 3
#define NUI_TOOLTIP_MARGIN_Y 2

bool nuiToolTip::SetRect(const nuiRect& rRect)
{
  nuiWidget::SetRect(rRect);
  nuiRect r(rRect.Size());
 
  r.Grow(-NUI_TOOLTIP_MARGIN_X, -NUI_TOOLTIP_MARGIN_Y);

  IteratorPtr pIt;
  for (pIt = GetFirstChild(); pIt && pIt->IsValid(); GetNextChild(pIt))
  {
    nuiWidgetPtr pItem = pIt->GetWidget();
    pItem->GetIdealRect();
    pItem->SetLayout(r);
  }
  delete pIt;

  return true;
}

nuiRect nuiToolTip::CalcIdealSize()
{
  nuiRect ideal(nuiSimpleContainer::CalcIdealSize());
  ideal.Grow(NUI_TOOLTIP_MARGIN_X, NUI_TOOLTIP_MARGIN_Y);
  return ideal;
}

bool nuiToolTip::Draw(nuiDrawContext* pContext)
{
  nuiRect rect = GetRect().Size();

  pContext->EnableBlending(true);
  pContext->SetBlendFunc(nuiBlendTransp);
  pContext->SetStrokeColor(GetColor(eToolTipBorder));
  pContext->SetFillColor(GetColor(eToolTipBg));
  pContext->DrawRect(rect,eStrokeAndFillShape);
  pContext->EnableBlending(false);

  DrawChildren(pContext);
  return true;
}

void nuiToolTip::SetText(const nglString& rText)
{
  mpLabel->SetText(rText);
  mpLabel->SetTextColor(GetColor(eToolTipFg));
}

nglString nuiToolTip::GetText() const
{
  if (!mpLabel)
    return nglString::Empty;
  return mpLabel->GetText();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
nuiTopLevel::nuiTopLevel(const nglPath& rResPath)
  : nuiSimpleContainer(),
    mToolTipTimerOn(0.5f),    
    mToolTipTimerOff(5.0f),
    mMessageQueueTimer(1.0/100.f),
    mpWatchedWidget(NULL),
    mFillTrash(false),
    mpDrawContext(NULL),
    mTopLevelSink(this),
    mpCSS(NULL)
{
  //EnableRenderCache(false);
  mReleased = false;
  mNeedInvalidateOnSetRect = false;
  mClearBackground = true;
  nuiTheme::InitTheme(rResPath);
  SetObjectClass(_T("nuiTopLevel"));

  mResPath = rResPath;

  mToolTipDelayOn = .5f;
  mToolTipDelayOff = 5.0f;
  
  mDisplayToolTip = false;
  mpToolTipSource = NULL;
  mpToolTipLabel = new nuiToolTip();
  AddChild(mpToolTipLabel);

  mTopLevelSink.Connect(mToolTipTimerOn.Tick, &nuiTopLevel::ToolTipOn);
  mTopLevelSink.Connect(mToolTipTimerOff.Tick, &nuiTopLevel::ToolTipOff);
  mTopLevelSink.Connect(mMessageQueueTimer.Tick, &nuiTopLevel::OnMessageQueueTick);
  mMessageQueueTimer.Start(false);

#ifdef _UIKIT_
  mDrawOrigin = nuiTop;
#endif

#ifdef _MULTI_TOUCHES_
  mpGrab.clear();
  mMouseInfo.mTouchId = -1;
#else
  mpGrab = NULL;
#endif
  mpFocus = NULL;
  mpUnderMouse = NULL;

  mLastClickedButton = nglMouseInfo::ButtonNone;

  EnablePartialRedraw(PARTIAL_REDRAW_DEFAULT);
  EnableRenderCache(false);

  SetWantKeyboardFocus(true);
  SetFocusVisible(false);
  SetMouseCursor(eCursorArrow);
}

bool nuiTopLevel::Load(const nuiXMLNode* pNode)
{
  mToolTipTimerOn.SetPeriod(0.5f);
  mToolTipTimerOff.SetPeriod(5.0f);
  mFillTrash = false;
  
  //EnableRenderCache(false);
  mClearBackground = true;
  mNeedInvalidateOnSetRect = false;
  mReleased = false;
  SetObjectClass(_T("nuiTopLevel"));
  mTopLevelSink.SetTarget(this);

  mResPath = nglPath(pNode->GetAttribute("Path"));
  nuiTheme::InitTheme(mResPath);

  mToolTipDelayOn = .5f;
  mToolTipDelayOff = 5.0f;

  mDisplayToolTip = false;
  mpToolTipSource = NULL;
  mpToolTipLabel = new nuiToolTip();
  AddChild(mpToolTipLabel);
  
  mTopLevelSink.Connect(mToolTipTimerOn.Tick, &nuiMainWindow::ToolTipOn);
  mTopLevelSink.Connect(mToolTipTimerOff.Tick, &nuiMainWindow::ToolTipOff);

#ifdef _UIKIT_
  mDrawOrigin = nuiTop;
#endif

#ifdef _MULTI_TOUCHES_
  mpGrab.clear();
  mMouseInfo.mTouchId = -1;
#else
  mpGrab = NULL;
#endif
  mpFocus = NULL;
  mpUnderMouse = NULL;

  mLastClickedButton = nglMouseInfo::ButtonNone;

  EnablePartialRedraw(PARTIAL_REDRAW_DEFAULT);
  EnableRenderCache(false);

  SetMouseCursor(eCursorArrow);
  
  return true;
}

nuiTopLevel::~nuiTopLevel()
{
  Exit();
}

void nuiTopLevel::Exit()
{
  if (mReleased)
    return;
  
//   mTopLevelSink.Disconnect(&nuiMainWindow::ToolTipOn);
//   mTopLevelSink.Disconnect(&nuiMainWindow::ToolTipOff);
  mTopLevelSink.DisconnectAll();

  mpToolTipLabel = NULL;
#ifdef _MULTI_TOUCHES_
  mpGrab.clear();
#else  
  mpGrab = NULL;
#endif
  mpFocus = NULL;
  mpUnderMouse = NULL;
  mpToolTipSource = NULL;
  mpToolTipLabel = NULL;
  mpInfoLabel = NULL;
  
  EmptyTrash();

  nuiWidgetList wlist(mpChildren);
  nuiWidgetList::iterator wit = wlist.begin();
  
  while (wlist.end() != wit)
  {
    nuiWidgetPtr pItem = *wit;
    pItem->CallOnTrash();
    nuiContainer* pCtr = pItem->GetParent();
    if (pCtr)
      pCtr->DelChild(pItem, false);
    delete pItem;
    
    ++wit;
  }

  mpChildren.clear();
  
  if (mpDrawContext)
    delete mpDrawContext;

  nuiTheme* pTheme = nuiTheme::GetTheme();
  pTheme->Release(); // Release once because of the Acquire in nuiTheme::GetTheme()
  pTheme->Release(); // Release twice to balance the nuiTheme::InitTheme(..).       
  
  mReleased = true;

  std::map<nglString, nuiHotKey*>::iterator it = mHotKeys.begin();
  std::map<nglString, nuiHotKey*>::iterator end = mHotKeys.end();

  while (it != end)
  {
    nuiHotKey* pHotKey = it->second;
    delete pHotKey;
    ++it;
  }
}

void nuiTopLevel::Trash(nuiWidgetPtr pWidget)
{
  nuiTrashElement Elem(nuiTrashElement::DeleteWidget,pWidget);
  mpTrash.remove(Elem);
  mpTrash.push_back(Elem);
}

bool nuiTopLevel::IsTrashFilling() const
{
  return mFillTrash;
}

void nuiTopLevel::FillTrash()
{
  EmptyTrash();
  mFillTrash = true;
}

void nuiTopLevel::EmptyTrash()
{
  BroadcastQueuedNotifications();
  UpdateWidgetsCSS();

  mFillTrash = false;

  std::list<nuiTrashElement>::iterator it = mpTrash.begin();
  std::list<nuiTrashElement>::iterator end = mpTrash.end();
  nuiWidgetPtr pItem = NULL;

  // Do the "DeleteWidget" opcode
  for (; it != end; ++it)
  {
    const nuiTrashElement& rElement = *it;
    if (rElement.mType == nuiTrashElement::DeleteWidget)
    {
      pItem = rElement.mpWidget;

      nuiContainerPtr pParent = pItem->GetParent();
      if (pParent)
        pParent->DelChild(pItem);

      //      OUT("About to delete %d (0x%x) from trash\n",pItem->GetProperty(_T("Class")), pItem);
			// Remove the complete subtree from the tree:
      AdviseSubTreeDeath(pItem);
      
      if (pItem != this)
        delete pItem;

    }
  }

  if (mpTrash.size()) // If we removed anything from the active objects then we have to redraw...
    Invalidate();

  mpTrash.clear();
  
  if (IsTrashed(false))
  {
    mTrashed = false;
    delete this;
  }
}

void nuiTopLevel::AdviseObjectDeath(nuiWidgetPtr pWidget)
{
  if (mpWatchedWidget == pWidget)
    mpWatchedWidget = NULL;
  
  mHoveredWidgets.erase(pWidget);
  
#ifdef _MULTI_TOUCHES_
  nglTouchId touchId;
  while ( (touchId = GetGrabId(pWidget)) >= 0 )
  {
    std::list<nuiWidgetPtr>& rGrabStack = GetGrabStack(touchId);
    rGrabStack.remove(pWidget);
    NGL_ASSERT(mpGrab.find(touchId) != mpGrab.end());
    mpGrab.erase(mpGrab.find(touchId));
  }
#else//!_MULTI_TOUCHES_
  if (mpGrab == pWidget)
  {
    mpGrabStack.remove(pWidget);
    mpGrab = NULL;
    Ungrab(pWidget);
  }
#endif//!_MULTI_TOUCHES_

  if (mpFocus == pWidget)
    mpFocus = NULL;
  if (mpUnderMouse == pWidget)
  {
    mpUnderMouse = NULL;
    GlobalHoverChanged();
  }
  if (mpToolTipSource == pWidget)
    ReleaseToolTip(pWidget);
  
  mCSSWidgets.erase(pWidget);
  
  {
    // Update the tab order containers:
    SetTabForward(pWidget, NULL, false);
    SetTabBackward(pWidget, NULL, false);
    
    {
      std::set<nuiWidgetPtr> ForwardSources;
      GetTabForwardSources(pWidget, ForwardSources);
      std::set<nuiWidgetPtr>::iterator it = ForwardSources.begin();
      std::set<nuiWidgetPtr>::iterator end = ForwardSources.end();
      while (it != end)
      {
        nuiWidgetPtr pSource = *it;
        SetTabForward(pSource, NULL, false);
        ++it;
      }
    }

    {
      std::set<nuiWidgetPtr> BackwardSources;
      GetTabBackwardSources(pWidget, BackwardSources);
      std::set<nuiWidgetPtr>::iterator it = BackwardSources.begin();
      std::set<nuiWidgetPtr>::iterator end = BackwardSources.end();
      while (it != end)
      {
        nuiWidgetPtr pSource = *it;
        SetTabBackward(pSource, NULL, false);
        ++it;
      }
    }
    
  }
  
}

void nuiTopLevel::AdviseSubTreeDeath(nuiWidgetPtr pWidget)
{
  IteratorPtr pIt;
  nuiContainer* pContainer = dynamic_cast<nuiContainer*> (pWidget);
  if (pContainer)
  {  
    for (pIt = pContainer->GetFirstChild(); pIt && pIt->IsValid(); pContainer->GetNextChild(pIt))
    {
      nuiWidgetPtr pItem = pIt->GetWidget();
      if (pItem)
        AdviseSubTreeDeath(pItem);
    }
    delete pIt;
  }

  AdviseObjectDeath(pWidget);
}

nuiDrawContext* nuiTopLevel::GetDrawContext()
{
  if (mpDrawContext)
    return mpDrawContext;

  nuiRect rect = GetRect().Size();
  mpDrawContext = nuiDrawContext::CreateDrawContext(rect, mRenderer, GetNGLContext());

#ifdef _UIKIT_
  mpDrawContext->GetPainter()->SetDrawOrigin(mDrawOrigin);
#endif

  return mpDrawContext;
}

void nuiTopLevel::SetDrawContext(nuiDrawContext* pDrawContext)
{
  if (mpDrawContext)
    delete mpDrawContext;

  mpDrawContext = pDrawContext;
}

nuiTopLevel* nuiTopLevel::GetTopLevel() const
{
  void* Self = (void*)this;
  return (nuiTopLevel*)Self;
}

nuiRenderer nuiTopLevel::mRenderer = eOpenGL;

void nuiTopLevel::SetRenderer(nuiRenderer Renderer)
{
  mRenderer = Renderer;
}

nuiRenderer nuiTopLevel::GetRenderer()
{
  return mRenderer;
}


/// Trash management operators:
bool operator==(const nuiTrashElement& rElement1,const nuiTrashElement& rElement2)
{
  return (rElement1.mpWidget == rElement2.mpWidget)
      && (rElement1.mpContainer == rElement2.mpContainer) 
      && (rElement1.mType == rElement2.mType);
}

nuiTrashElement::nuiTrashElement(nuiTrashElement::ElementType type, nuiWidgetPtr pWidget, nuiContainerPtr pContainer )
{
  mpWidget = pWidget;
  mpContainer = pContainer;
  mType = type;
}

nuiTrashElement::nuiTrashElement(const nuiTrashElement& rSrc)
{
  mpWidget = rSrc.mpWidget;
  mpContainer = rSrc.mpContainer;
  mType = rSrc.mType;
}

nuiTrashElement::~nuiTrashElement()
{
}

bool nuiTopLevel::Grab(nuiWidgetPtr pWidget)
{
  NGL_TOUCHES_DEBUG( NGL_OUT(_T("nuiTopLevel::Grab 0x%x\n"), pWidget) );

#ifdef _MULTI_TOUCHES_

///< some widgets acquire the grab on creation, which is pretty unpleasant (this hack is quite bad)
  nglTouchId touchId = mMouseInfo.mTouchId;
  if (touchId < 0) {
    return false;
  }
  
  if (pWidget && !pWidget->AcceptsMultipleGrabs()
///< Checks if the Widget has been grabbed by another touches
      && HasGrab(pWidget))
  {

  NGL_TOUCHES_DEBUG( NGL_OUT(_T("TouchId[%d] "), mMouseInfo.mTouchId) );
  NGL_TOUCHES_DEBUG( NGL_OUT(_T("%ls of type %ls already grabbed on touch id[%d]\n"), 
          pWidget->GetProperty(_T("Name")).GetChars(),
          pWidget->GetProperty(_T("Class")).GetChars(), GetGrabId(pWidget)) );

    return false;
  }
  if (pWidget) {
    NGL_TOUCHES_DEBUG( NGL_OUT(_T("TouchId[%d] "), mMouseInfo.mTouchId) );
    NGL_TOUCHES_DEBUG( NGL_OUT(_T("Grab from %ls of type %ls\n"),
            pWidget->GetProperty(_T("Name")).GetChars(), pWidget->GetProperty(_T("Class")).GetChars()) );
  }

  nuiWidgetPtr pGrab = GetGrab();
  
  if (pGrab)
  {
    if (pWidget) {
      PushGrab(touchId, pGrab);
    }
    pGrab->MouseUngrabbed();
  }

  pGrab = pWidget;
  mpGrab[touchId] = pWidget;
  if (pGrab)
    pGrab->MouseGrabbed();

  if (pGrab && pGrab->GetProperty("ToolTipOnGrab") == _T("true"))
  {
    mpToolTipSource = pGrab;
    SetToolTipOn(false);
  }
  else
    mDisplayToolTip = false;

#else//!_MULTI_TOUCHES_

  if (mpGrab)
  {
    if (pWidget)
      mpGrabStack.push_back(mpGrab);
    mpGrab->MouseUngrabbed();
  }

  mpGrab = pWidget;
  if (mpGrab)
    mpGrab->MouseGrabbed();

  if (mpGrab && mpGrab->GetProperty("ToolTipOnGrab") == _T("true"))
  {
    mpToolTipSource = mpGrab;
    SetToolTipOn(false);
  }
  else
    mDisplayToolTip = false;

#endif//!_MULTI_TOUCHES_

  return true;
}

bool nuiTopLevel::Ungrab(nuiWidgetPtr pWidget)
{
NGL_TOUCHES_DEBUG( NGL_OUT(_T("nuiTopLevel::Ungrab 0x%x\n"), pWidget) );
if (pWidget) {
//  NGL_TOUCHES_DEBUG( NGL_OUT(_T("TouchId[%d] "), mMouseInfo.mTouchId) );
  NGL_TOUCHES_DEBUG( NGL_OUT(_T("Ungrab from %ls of type %ls\n"),
          pWidget->GetProperty(_T("Name")).GetChars(), pWidget->GetProperty(_T("Class")).GetChars()) );
}

#ifdef _MULTI_TOUCHES_

  nuiWidgetPtr pGrab = GetGrab();
  if (pGrab && pGrab->GetProperty("ToolTipOnGrab") == _T("true"))
  {
    mpToolTipSource = NULL;
    mDisplayToolTip = false;
  }

  std::list<nuiWidgetPtr>& rGrabStack = GetGrabStack(mMouseInfo.mTouchId);
  if (rGrabStack.size() && pGrab == pWidget)
  {
    if (pGrab)
      pGrab->MouseUngrabbed();
    pGrab = PopGrab(mMouseInfo.mTouchId);
    if (pGrab)
      pGrab->MouseGrabbed();
  }
  else
  {
    Grab(NULL);
  }

#else//!_MULTI_TOUCHES_

  if (mpGrab && mpGrab->GetProperty("ToolTipOnGrab") == _T("true"))
  {
    mpToolTipSource = NULL;
    mDisplayToolTip = false;
  }

  if (mpGrabStack.size() && mpGrab == pWidget)
  {
    if (mpGrab)
      mpGrab->MouseUngrabbed();
    mpGrab = mpGrabStack.back();
    mpGrabStack.pop_back();
    if (mpGrab)
      mpGrab->MouseGrabbed();
  }
  else
  {
    Grab(NULL);
  }
#endif//_MULTI_TOUCHES_
  return true;
}

bool nuiTopLevel::CancelGrab()
{
NGL_TOUCHES_DEBUG( NGL_OUT(_T("CancelGrab()\n")) );
#ifdef _MULTI_TOUCHES_
  for (nuiGrabMap::const_iterator it = mpGrab.begin(); it != mpGrab.end(); ++it)
  {
    nglTouchId touchId = it->first;
    nuiWidget* pGrab = it->second;
    std::list<nuiWidgetPtr>& rGrabStack = GetGrabStack(touchId);

    while (pGrab)
    {
      if (pGrab->GetProperty("ToolTipOnGrab") == _T("true"))
      {
        mpToolTipSource = NULL;
        mDisplayToolTip = false;
      }
      pGrab->MouseUngrabbed();
      pGrab = NULL;

      if (rGrabStack.size()) {
        pGrab = PopGrab(touchId);
      }
    }
    NGL_ASSERT(rGrabStack.empty());
  }
  mpGrab.clear();
  mpGrabStack.clear();
#else//!_MULTI_TOUCHES_
  while (mpGrab)
  {
    if (mpGrab->GetProperty("ToolTipOnGrab") == _T("true"))
    {
      mpToolTipSource = NULL;
      mDisplayToolTip = false;
    }
    mpGrab->MouseUngrabbed();

    if (mpGrabStack.empty())
      mpGrab = NULL;
    else
    {
      mpGrab = mpGrabStack.back();
      mpGrabStack.pop_back();
    }
  }
#endif
  return true;
}


#ifdef _MULTI_TOUCHES_

bool nuiTopLevel::HasGrab(nuiWidgetPtr pWidget)
{
  if (GetGrabId(pWidget) >= 0)
    return true;
  return false;
}

std::list<nuiWidgetPtr>& nuiTopLevel::GetGrabStack(nglTouchId touchId)
{
  nuiGrabStackMap::iterator it = mpGrabStack.find(touchId);
  if (it != mpGrabStack.end())
    return it->second;
  mpGrabStack[touchId] = std::list<nuiWidgetPtr>();
  return mpGrabStack[touchId];
}

nglTouchId nuiTopLevel::GetGrabId(nuiWidgetPtr pWidget) const
{
  nuiGrabMap::const_iterator end = mpGrab.end();
  for (nuiGrabMap::const_iterator it = mpGrab.begin(); it != end; ++it)
  {
    if (it->second == pWidget)
      return it->first;
  }
  return (nglTouchId)-1;
}

void nuiTopLevel::PushGrab(nglTouchId touchId, nuiWidgetPtr pWidget)
{
  NGL_ASSERT(pWidget);
  NGL_TOUCHES_DEBUG( NGL_OUT(_T("Push[%d] Grabstack: [%ls] of type [%ls]\n"),
          pWidget->GetProperty(_T("Name")).GetChars(), pWidget->GetProperty(_T("Class")).GetChars()) );
  mpGrabStack[touchId].push_back(pWidget);
}

nuiWidgetPtr nuiTopLevel::PopGrab(nglTouchId touchId)
{
  NGL_TOUCHES_DEBUG( NGL_OUT(_T("Pop[%d] Grabstack\n"), touchId) );

  std::list<nuiWidgetPtr>& rGrabStack = GetGrabStack(touchId);
  if (rGrabStack.empty())
    return NULL;

  nuiWidgetPtr pWidget = rGrabStack.back();
  NGL_TOUCHES_DEBUG( NGL_OUT(_T("->%ls of type %ls\n"),
                              pWidget->GetProperty(_T("Name")).GetChars(),
                              pWidget->GetProperty(_T("Class")).GetChars()) );
  rGrabStack.pop_back();
  return pWidget;
}

nuiWidgetPtr nuiTopLevel::GetGrab(nglTouchId touchId) const
{
  nuiGrabMap::const_iterator it = mpGrab.find(touchId);
  if (it != mpGrab.end())
    return it->second;
  return NULL;
}
#endif

nuiWidgetPtr nuiTopLevel::GetGrab() const
{
#ifdef _MULTI_TOUCHES_
  return GetGrab(mMouseInfo.mTouchId);
#else
  return mpGrab;
#endif
}

bool nuiTopLevel::SetFocus(nuiWidgetPtr pWidget)
{
  if (mpFocus == pWidget)
    return true;

  nuiWidget* pOldFocus = mpFocus;
  
  if (mpFocus)
    mpFocus->DispatchFocus(pWidget);
  if (pWidget)
    pWidget->DispatchFocus(pWidget);

  mpFocus = pWidget;

  if (pOldFocus)
    pOldFocus->Invalidate();
  if (mpFocus)
    mpFocus->Invalidate();
  
  return true;
}

nuiWidgetPtr nuiTopLevel::GetFocus() const
{
  return mpFocus;
}

bool nuiTopLevel::ActivateToolTip(nuiWidgetPtr pWidget, bool Now)
{
  if (pWidget == this)
  {
    nglString tt = GetToolTip();
  
    if (tt.IsEmpty())
      return false;
  }

  mpToolTipSource = pWidget;
  mToolTipTimerOn.SetPeriod(mToolTipDelayOn);
  mToolTipTimerOn.Stop();
  mToolTipTimerOn.Start(Now);
  mDisplayToolTip = Now;
  if (Now)
  {
    ToolTipOn(NULL);
  }

  return true;
}

bool nuiTopLevel::ReleaseToolTip(nuiWidgetPtr pWidget)
{
  if (mpToolTipSource == pWidget)
  {
    mToolTipTimerOn.Stop();
    mToolTipTimerOff.Stop();
    mDisplayToolTip = false;
    if (mpToolTipLabel)
    {
      mpToolTipLabel->SetVisible(false);
    }
    mpToolTipSource = NULL;
    //Invalidate();
    mpToolTipLabel->Invalidate();
  }

  return true;
}

void nuiTopLevel::SetToolTipOn(bool AutoStop)
{
  if (mpToolTipSource)
  {
    /*
    NGL_OUT(_T("ToolTipOn class %ls / Text: %ls\n"),
        mpToolTipSource->GetProperty(_T("Class")).GetChars(),
        mpToolTipSource->GetToolTip().GetChars()
       );
       */
  }

  mDisplayToolTip = true;
  mToolTipTimerOff.SetPeriod(mToolTipDelayOff);
  mToolTipTimerOn.Stop();
  mToolTipTimerOff.Stop();
  if (AutoStop)
  {
    mToolTipTimerOff.Start(false);
  }
  //Invalidate();
  SetToolTipRect();
}

bool nuiTopLevel::ToolTipOn(const nuiEvent& rEvent)
{
  if (mpToolTipSource)
  {
    /*
    NGL_OUT(_T("ToolTipOn class %ls / Text: %ls\n"),
        mpToolTipSource->GetProperty(_T("Class")).GetChars(),
        mpToolTipSource->GetToolTip().GetChars()
       );
       */
  }

  mDisplayToolTip = true;
  mToolTipTimerOff.SetPeriod(mToolTipDelayOff);
  mToolTipTimerOn.Stop();
  mToolTipTimerOff.Stop();
  mToolTipTimerOff.Start(false);
  
  //Invalidate();
  SetToolTipRect();
  return false;
}

bool nuiTopLevel::ToolTipOff(const nuiEvent& rEvent)
{
  //NGL_OUT(_T("ToolTipOff\n"));
  mDisplayToolTip = false;
  mToolTipTimerOn.Stop();
  mToolTipTimerOff.Stop();
  //Invalidate();
  mpToolTipLabel->Invalidate();

  return false;
}

bool nuiTopLevel::IsKeyDown (nglKeyCode Key) const
{
  return false;
}

bool nuiTopLevel::CallTextInput (const nglString& rUnicodeText)
{
  if (mpFocus && mpFocus->IsEnabled())
  {
    if (mpFocus->DispatchTextInput(rUnicodeText))
    {
      return true;
    }
  }
  else
  {
    if (DispatchTextInput(rUnicodeText))
      return true;
  }
  
  return false;
}


bool nuiTopLevel::CallKeyDown (const nglKeyEvent& rEvent)
{
  if (mpFocus)
  {
    if (mpFocus->IsEnabled())
    {
      if (mpFocus->DispatchKeyDown(rEvent))
      {
        return true;
      }
    }
  }
  else
  {
    if (DispatchKeyDown(rEvent))
      return true;
  }

  return false;
}

bool nuiTopLevel::CallKeyUp (const nglKeyEvent& rEvent)
{
  if (mpFocus && mpFocus->IsEnabled())
  {
    if (mpFocus->DispatchKeyUp(rEvent))
    {
      return true;
    }
  }
  else
  {
    if (DispatchKeyUp(rEvent))
      return true;
  }

  return false;
}

#ifdef _UIKIT_
void AdjustFromDrawOrigin(nuiPosition DrawOrigin, const nuiRect& rRect, nglMouseInfo& rInfo)
{
  switch (DrawOrigin)
  {
    case nuiLeft: {
      int tmpX= rInfo.X;
      rInfo.X = ((int)rRect.GetHeight()) - rInfo.Y;
      rInfo.Y = tmpX;
    } break;
    case nuiRight: {
      int tmpY = rInfo.Y;
      rInfo.Y = ((int)rRect.GetWidth()) - rInfo.X;
      rInfo.X = tmpY;
    } break;
    case nuiBottom: {
      rInfo.X = ((int)rRect.GetWidth()) - rInfo.X;
      rInfo.Y = ((int)rRect.GetHeight()) - rInfo.Y;
    } break;
    case nuiTop:
    break;
  }
}
#endif

bool nuiTopLevel::CallMouseClick (nglMouseInfo& rInfo)
{
#ifdef _UIKIT_
  AdjustFromDrawOrigin(mDrawOrigin, mRect, rInfo);
  mMouseInfo.SwipeInfo = rInfo.SwipeInfo;
#endif

  mMouseInfo.X = rInfo.X;
  mMouseInfo.Y = rInfo.Y;
  mMouseInfo.Buttons |= rInfo.Buttons;

#ifdef _MULTI_TOUCHES_

NGL_TOUCHES_DEBUG( NGL_OUT(_T("nuiTopLevel::CallMouseClick X:%d Y:%d\n"), rInfo.X, rInfo.Y) );

  mMouseInfo.mTouchId = rInfo.mTouchId;

NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseClick [%d] BEGIN\n"), rInfo.mTouchId) );

  nuiWidgetPtr pGrab = GetGrab();
  if (pGrab)
  {
		bool res = false;
    if (pGrab->MouseEventsEnabled())
    {
      res = pGrab->DispatchMouseClick((nuiSize)rInfo.X, (nuiSize)rInfo.Y, rInfo.Buttons);
    }

PRINT_GRAB_IDS();
NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseClick [%d] END\n"), rInfo.mTouchId) );

    return res;
  }
#else//!_MULTI_TOUCHES_
  if (mpGrab)
  {
		bool res = false;
    if (mpGrab->MouseEventsEnabled())
    {
      res = mpGrab->DispatchMouseClick((nuiSize)rInfo.X, (nuiSize)rInfo.Y, rInfo.Buttons);
    }
    return res;
  }
#endif//!_MULTI_TOUCHES_

  bool res = DispatchMouseClick((nuiSize)rInfo.X, (nuiSize)rInfo.Y, rInfo.Buttons);
  nuiWidget* pWidgetUnder = GetChild((nuiSize)rInfo.X, (nuiSize)rInfo.Y);
  if (pWidgetUnder)
    SetMouseCursor(pWidgetUnder->GetMouseCursor());
  else
    SetMouseCursor(GetMouseCursor());

#ifdef _MULTI_TOUCHES_
PRINT_GRAB_IDS();
NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseClick [%d] END\n"), rInfo.mTouchId) );
#endif//_MULTI_TOUCHES_

	return res;
}

bool nuiTopLevel::CallMouseUnclick(nglMouseInfo& rInfo)
{
#ifdef _UIKIT_
  AdjustFromDrawOrigin(mDrawOrigin, mRect, rInfo);
  mMouseInfo.SwipeInfo = rInfo.SwipeInfo;
#endif

//  NGL_TOUCHES_DEBUG( NGL_OUT(_T("nuiTopLevel::CallMouseUnclick X:%d Y:%d\n"), rInfo.X, rInfo.Y) );

  mMouseInfo.X = rInfo.X;
  mMouseInfo.Y = rInfo.Y;
  mMouseInfo.Buttons &= ~rInfo.Buttons;
  nglMouseInfo::Flags Buttons = rInfo.Buttons | mLastClickedButton & nglMouseInfo::ButtonDoubleClick;

#ifdef _MULTI_TOUCHES_
  mMouseInfo.mTouchId = rInfo.mTouchId;
NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseUnclick [%d] BEGIN\n"), rInfo.mTouchId) );
#endif//!_MULTI_TOUCHES_
  nuiWidgetPtr pGrab = GetGrab();
  if (pGrab)
  {
		bool res = false;
    if (pGrab->MouseEventsEnabled())
    {
      res = pGrab->DispatchMouseUnclick((nuiSize)rInfo.X, (nuiSize)rInfo.Y, Buttons);
    }
#ifdef _MULTI_TOUCHES_
PRINT_GRAB_IDS();
NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseUnclick [%d] END\n"), rInfo.mTouchId) );
#endif//_MULTI_TOUCHES_
    return res ;
  }

  bool res = DispatchMouseUnclick((nuiSize)rInfo.X, (nuiSize)rInfo.Y, Buttons);
  nuiWidget* pWidgetUnder = GetChild((nuiSize)rInfo.X, (nuiSize)rInfo.Y);
  if (pWidgetUnder)
    SetMouseCursor(pWidgetUnder->GetMouseCursor());
  else
    SetMouseCursor(GetMouseCursor());

#ifdef _MULTI_TOUCHES_
PRINT_GRAB_IDS();
NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseUnclick [%d] END\n"), rInfo.mTouchId) );
#endif//_MULTI_TOUCHES_

	return res;
}

void UpdateHoverList(nuiContainer* pContainer, nuiSize X, nuiSize Y, std::set<nuiWidget*>& rHoverSet, std::list<nuiWidget*>& rHoverList)
{
  nuiContainer::IteratorPtr pIt = NULL;
  for (pIt = pContainer->GetFirstChild(); pIt && pIt->IsValid(); pContainer->GetNextChild(pIt))
  {
    nuiWidgetPtr pItem = pIt->GetWidget();
    if (pItem->IsInside(X, Y))
    {
      rHoverList.push_back(pItem);
      rHoverSet.insert(pItem);
      nuiContainer* pChild = dynamic_cast<nuiContainer*>(pItem);
      if (pChild)
        UpdateHoverList(pChild, X, Y, rHoverSet, rHoverList);
    }
  }
  delete pIt;
}

void nuiTopLevel::UpdateHoverList(nglMouseInfo& rInfo)
{
#ifdef _MULTI_TOUCHES_
//  if (GetGrab())
    return;
#else//!_MULTI_TOUCHES_
  if (mpGrab)
  {
    return;
  }
#endif//!_MULTI_TOUCHES_

  std::list<nuiWidget*> HoverList;
  std::set<nuiWidget*> HoverSet;
  HoverSet.insert(this);
  ::UpdateHoverList(this, rInfo.X, rInfo.Y, HoverSet, HoverList);
  
  // Old Hovered widgets:
  std::set<nuiWidget*> OldHover;
  std::insert_iterator<std::set<nuiWidget*> > oldit(OldHover, OldHover.begin());
  set_difference(mHoveredWidgets.begin(), mHoveredWidgets.end(), HoverSet.begin(), HoverSet.end(), oldit);

  {
    std::set<nuiWidget*>::iterator it = OldHover.begin();
    std::set<nuiWidget*>::iterator end = OldHover.end();
    
    while (it != end)
    {
      nuiWidget* pItem = *it;
      pItem->SetHover(false);
      ++it;
    }
  }
  
  // New Hovered widgets:
  std::set<nuiWidget*> NewHover;
  std::insert_iterator<std::set<nuiWidget*> > newit(NewHover, NewHover.begin());
  set_difference(HoverSet.begin(), HoverSet.end(), mHoveredWidgets.begin(), mHoveredWidgets.end(), newit);

  {
    std::set<nuiWidget*>::iterator it = NewHover.begin();
    std::set<nuiWidget*>::iterator end = NewHover.end();
    
    while (it != end)
    {
      nuiWidget* pItem = *it;
      pItem->SetHover(true);
      ++it;
    }
  }
  
  mHoveredWidgets = HoverSet;
  
  // Update Tooltip:
  if (HoverList.empty())
    return;
  
  HoverList.reverse();
  std::list<nuiWidget*>::iterator tt = HoverList.begin();
  std::list<nuiWidget*>::iterator ttend = HoverList.end();

  bool res = false;
  while ((tt != ttend) && !(res = (*tt)->ActivateToolTip(*tt)))
    ++tt;
  if (!res && mpToolTipSource)
    ReleaseToolTip(mpToolTipSource);
}


bool nuiTopLevel::CallMouseMove (nglMouseInfo& rInfo)
{
#ifdef _UIKIT_
  AdjustFromDrawOrigin(mDrawOrigin, mRect, rInfo);
  mMouseInfo.SwipeInfo = rInfo.SwipeInfo;
#endif

NGL_TOUCHES_DEBUG( NGL_OUT(_T("nuiTopLevel::CallMouseMove X:%d Y:%d\n"), rInfo.X, rInfo.Y) );

  mMouseInfo.X = rInfo.X;
  mMouseInfo.Y = rInfo.Y;

#ifdef _MULTI_TOUCHES_
  mMouseInfo.mTouchId = rInfo.mTouchId;
NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseMove [%d] BEGIN\n"), rInfo.mTouchId) );
#endif

  nuiWidgetPtr pWidget = NULL;
  nuiWidgetPtr pWidgetUnder = NULL;

  if (mToolTipTimerOn.IsRunning())
  {
    mToolTipTimerOn.Stop();
    mToolTipTimerOn.Start(false);
  }
	
	nuiWidgetPtr pHandled = NULL;

  // If there is a grab on the mouse serve the grabbing widget and only this one:
  nuiWidgetPtr pGrab = GetGrab();
  if (pGrab)
  {
    //NGL_OUT(_T("grabbed mouse move on '%ls' / '%ls'\n"), mpGrab->GetObjectClass().GetChars(), mpGrab->GetObjectName().GetChars());
#ifdef _MULTI_TOUCHES_
    NGL_ASSERT(mpGrab[mMouseInfo.mTouchId]);
    if (mpGrab[mMouseInfo.mTouchId]->MouseEventsEnabled())
#else
    if (mpGrab->MouseEventsEnabled())
#endif
    {
      pHandled = pGrab->DispatchMouseMove((nuiSize)rInfo.X, (nuiSize)rInfo.Y);
    }
    
    nuiWidgetPtr pChild = NULL;
    // There might be no grab object left after the mouse move event!
    if (pGrab) 
      pChild = pGrab;
    else             
      pChild = GetChild((nuiSize)rInfo.X, (nuiSize)rInfo.Y);

    NGL_ASSERT(pChild);
    // Set the mouse cursor to the right object:
    SetMouseCursor(pChild->GetMouseCursor());
    SetToolTipRect();
    
//NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseMove [%d] END\n"), rInfo.mTouchId) );
    return pHandled != NULL;
  }
  else
  { /// this is a mouse over event
    UpdateHoverList(rInfo);
    nuiSize x,y;

    IteratorPtr pIt;
    for (pIt = GetLastChild(); pIt && pIt->IsValid() && !pHandled; GetPreviousChild(pIt))
    {
      // First find the widget directly under the mouse:
      nuiWidgetPtr pPtr = pIt->GetWidget();

      pWidgetUnder = pPtr->GetChild((nuiSize)rInfo.X, (nuiSize)rInfo.Y);
      pWidget = pWidgetUnder;
      if (pWidget)
      {
        // As long as there is no widget that handles the event, try to find one:
        while (pWidget && !pHandled)
        {
          x=(nuiSize)rInfo.X;
          y=(nuiSize)rInfo.Y;
          if (pWidget->MouseEventsEnabled())
          {
            pHandled = pWidget->DispatchMouseMove(x,y);
          }
          if (!pHandled)
            pWidget = pWidget->GetParent();
          else 
          { 
            pWidgetUnder = pWidget = pHandled;
            break;
          }
        }
      }
    }
    delete pIt;
    
    if (mpUnderMouse && mpUnderMouse != pWidget)
    {
      x = (nuiSize)rInfo.X;
      y = (nuiSize)rInfo.Y;
      if (mpUnderMouse->MouseEventsEnabled())
      {
        mpUnderMouse->DispatchMouseMove(x,y);
      }
    }

    //if (pHandled)
    {
      if (mpUnderMouse != pWidget)
      {
        mpUnderMouse = pWidget;
        GlobalHoverChanged();
      }
    }
  }

  if (pWidgetUnder)
    SetMouseCursor(pWidgetUnder->GetMouseCursor());
  else
    SetMouseCursor(GetMouseCursor());

  SetToolTipRect();
//NGL_TOUCHES_DEBUG( NGL_OUT(_T("CallMouseMove [%d] END\n"), rInfo.mTouchId) );
  return pHandled != NULL;
}

void nuiTopLevel::SetToolTipRect()
{
  if (mDisplayToolTip && mpToolTipSource)
  {
    nglString text(mpToolTipSource->GetToolTip());

    if (text.IsEmpty())
    {
      if (mpToolTipLabel->IsVisible())
      {
        mpToolTipLabel->SetVisible(false);
      }
      return;
    }

    // Place the tool tip:
    if (!mpToolTipLabel->IsVisible())
    {
      mpToolTipLabel->SetVisible(true);
    }


    if (mpToolTipLabel->GetText() != text)
    {
      mpToolTipLabel->SetText(text);
    }

    nuiRect ttrect(mpToolTipLabel->GetIdealRect());

    nglMouseInfo mouse;
    GetMouseInfo(mouse);

    if ((mpToolTipSource->HasProperty("ToolTipFollowMouse") && nuiGetBool(mpToolTipSource->GetProperty("ToolTipFollowMouse"), false))
      || !mpToolTipSource->HasProperty("ToolTipFollowMouse") )
    {
      // Position the tooltip relatively to the mouse
      if (mpToolTipSource->HasProperty("ToolTipPosition"))
      {
        nuiPosition pos = nuiGetPosition(mpToolTipSource->GetProperty("ToolTipPosition"), nuiBottomRight);
        nuiRect tt(mouse.X, mouse.Y, 0, 0);
        tt.Grow(ttrect.GetWidth() + 16, ttrect.GetHeight() + 16);
        ttrect.SetPosition(pos, tt);
      }
      else
      {
        ttrect.Set
          (
          (nuiSize)(mouse.X+16),
          (nuiSize)(mouse.Y+16),
          ttrect.GetWidth(), 
          ttrect.GetHeight()
          );
      }
    }
    else
    {
      // Position the tooltip relatively to the tooltip source widget
      nuiPosition pos = nuiBottom;
      if (mpToolTipSource->HasProperty("ToolTipPosition"))
        pos = nuiGetPosition(mpToolTipSource->GetProperty("ToolTipPosition"), pos);
      nuiRect tt(mpToolTipSource->GetRect().Size());
      mpToolTipSource->LocalToGlobal(tt);
      tt.Grow(ttrect.GetWidth(), ttrect.GetHeight());
      ttrect.SetPosition(pos, tt);
    }
    ttrect.RoundToNearest();
    // do not display tooltip out of the topLevel borders
    ttrect.MoveTo(MIN(mRect.GetWidth()-ttrect.GetWidth(), ttrect.Left()), MIN(mRect.GetHeight()-ttrect.GetHeight(), ttrect.Top()));

    mpToolTipLabel->Invalidate();
    mpToolTipLabel->SetLayout(ttrect);
    InvalidateRect(ttrect);
  }
  else
  {
    if (mpToolTipLabel && mpToolTipLabel->IsVisible())
    {
      mpToolTipLabel->SetVisible(false);
    }
  }
}

void nuiTopLevel::DisplayToolTips(nuiDrawContext* pContext)
{
  if (mDisplayToolTip && mpToolTipSource && mpToolTipLabel)
  {
    DrawChild(pContext, mpToolTipLabel);
  }
}


static const bool DISPLAY_PARTIAL_RECTS = false;

bool nuiTopLevel::Draw(class nuiDrawContext *pContext)
{
  uint32 clipWidth, clipHeight;
#ifdef _UIKIT_
  if (mDrawOrigin == nuiLeft || mDrawOrigin == nuiRight) {
    clipWidth=pContext->GetHeight();
    clipHeight=pContext->GetWidth();
  }
  else
#endif
  {
    clipWidth=pContext->GetWidth();
    clipHeight=pContext->GetHeight();
  }

  if (mPartialRedraw && !DISPLAY_PARTIAL_RECTS)
  {
    // Prepare the layout changes:
    pContext->ResetState();

#if 0
    pContext->EnableClipping(false);
    pContext->EnableBlending(true);
    pContext->SetBlendFunc(nuiBlendTransp);
    pContext->SetFillColor(nuiColor(.1f,.1f,.1f, .1f));
    pContext->DrawRect(GetRect().Size(), eFillShape);
#endif

#if 0
    pContext->ResetState();
    pContext->ResetClipRect();
    pContext->ResetClipShape();
    pContext->SetStrokeColor(nuiColor(1.0f,0.0f,0.0f,0.0f));
    pContext->SetFillColor(nuiColor(1.0f,0.0f,0.0f,.5f));
    pContext->EnableBlending(true);
    pContext->SetBlendFunc(nuiBlendTransp);
    pContext->DrawRect(GetRect().Size(), eFillShape);
/*
    for (int i=0; i < count; i++)
    {
      pContext->DrawRect(mRedrawList[i], eStrokeAndFillShape);
    }
*/
    pContext->ResetState();
#endif

    // Update the widget informations:
    {
      nuiPainter* pPainter = pContext->GetPainter();
      nuiMetaPainter painter(nuiRect(0,0, clipWidth, clipHeight), NULL);
      painter.SetDummyMode(true);
      pContext->SetPainter(&painter);

      pContext->ResetState();
      pContext->ResetClipRect();
      pContext->ResetClipShape();
      pContext->SetStrokeColor(nuiColor(1.0f,0.0f,0.0f,0.0f));
      pContext->SetFillColor(nuiColor(1.0f,0.0f,0.0f,.5f));
      pContext->EnableBlending(true);
      pContext->SetBlendFunc(nuiBlendTransp);
      pContext->EnableClipping(true);

      IteratorPtr pIt = NULL;
      for (pIt = GetFirstChild(); pIt && pIt->IsValid(); GetNextChild(pIt))
      {
        nuiWidgetPtr pItem = pIt->GetWidget();
        if (pItem && pItem != mpToolTipLabel)
          DrawChild(pContext, pItem);
      }
      delete pIt;

      DisplayToolTips(pContext);
      pContext->SetPainter(pPainter);
    }

    // Ok, all the widgets descriptions should be ok now, let's draw them:

    int count = mRedrawList.size();

    for (int i=0; i < count; i++)
    {
      pContext->ResetState();
      pContext->ResetClipRect();
      pContext->ResetClipShape();
      pContext->SetStrokeColor(nuiColor(1.0f,0.0f,0.0f,0.0f));
      pContext->SetFillColor(nuiColor(1.0f,0.0f,0.0f,.5f));
      pContext->Clip(mRedrawList[i]);
      pContext->EnableClipping(true);

      if (mClearBackground)
      {
        pContext->SetClearColor(GetColor(eActiveWindowBg));
        pContext->Clear();
      }

      IteratorPtr pIt;
      for (pIt = GetFirstChild(); pIt && pIt->IsValid(); GetNextChild(pIt))
      {
        nuiWidgetPtr pItem = pIt->GetWidget();
        if (pItem && pItem != mpToolTipLabel)
          DrawChild(pContext, pItem);
      }
      delete pIt;
      
      DisplayToolTips(pContext);
    }

#if 0
    pContext->ResetState();
    pContext->ResetClipRect();
    pContext->ResetClipShape();
    pContext->SetStrokeColor(nuiColor(1.0f,0.0f,0.0f,0.0f));
    pContext->SetFillColor(nuiColor(1.0f,0.0f,0.0f,.5f));
    pContext->EnableBlending(true);
    pContext->SetBlendFunc(nuiBlendTransp);
    for (int i=0; i < count; i++)
    {
      pContext->DrawRect(mRedrawList[i], eStrokeAndFillShape);
    }
    pContext->ResetState();
#endif

  }
  else
  {
    pContext->ResetState();
    pContext->ResetClipRect();

    if (mClearBackground)
    {
      pContext->SetClearColor(GetColor(eActiveWindowBg));
      pContext->Clear();
    }

    IteratorPtr pIt = NULL;
    for (pIt = GetFirstChild(); pIt && pIt->IsValid(); GetNextChild(pIt))
    {
      nuiWidgetPtr pItem = pIt->GetWidget();
      if (pItem && pItem != mpToolTipLabel)
        DrawChild(pContext, pItem);
    }
    delete pIt;
    
    DisplayToolTips(pContext);

    if (DISPLAY_PARTIAL_RECTS)
    {
      int count = mRedrawList.size();
      pContext->ResetState();
      pContext->ResetClipRect();
      pContext->ResetClipShape();
      pContext->SetStrokeColor(nuiColor(1.0f,0.0f,0.0f,0.0f));
      pContext->SetFillColor(nuiColor(1.0f,0.0f,0.0f,.5f));
      pContext->EnableBlending(true);
      pContext->SetBlendFunc(nuiBlendTransp);
      for (int i=0; i < count; i++)
      {
        pContext->DrawRect(mRedrawList[i], eStrokeAndFillShape);
      }
      pContext->ResetState();    
    }
    
#if 0
    pContext->ResetState();
    pContext->ResetClipRect();
    pContext->ResetClipShape();
    pContext->SetStrokeColor(nuiColor(0.0f,0.0f,1.0f,0.0f));
    pContext->SetFillColor(nuiColor(0.0f,0.0f,1.0f,.5f));
    pContext->EnableBlending(true);
    pContext->SetBlendFunc(nuiBlendTransp);
    pContext->DrawRect(mRect.Size(), eStrokeAndFillShape);
    pContext->ResetState();
#endif
  }

  mRedrawList.clear();

  if (mpWatchedWidget)
  {
    nuiRect r(mpWatchedWidget->GetRect().Size());
    mpWatchedWidget->LocalToGlobal(r);
    
    pContext->ResetState();
    pContext->ResetClipRect();
    pContext->ResetClipShape();
    pContext->SetStrokeColor(nuiColor(0.0f,0.0f,1.0f,0.5f));
    pContext->SetFillColor(nuiColor(0.0f,0.0f,1.0f,.25f));
    pContext->EnableBlending(true);
    pContext->SetBlendFunc(nuiBlendTransp);
    pContext->DrawRect(r, eStrokeAndFillShape);
    pContext->ResetState();
  }
  
  return true;
}
                     
void nuiTopLevel::GetMouseInfo(nglMouseInfo& rMouseInfo) const
{
  rMouseInfo = mMouseInfo;
}

nglPath nuiTopLevel::GetResourcePath() const
{
  return mResPath;
}


void nuiTopLevel::BroadcastInvalidateRect(nuiWidgetPtr pSender, const nuiRect& rRect)
{
  nuiRect r = rRect;
  nuiRect rect = GetRect();

#ifdef _UIKIT_
  if (mDrawOrigin == nuiLeft || mDrawOrigin == nuiRight)
    rect.SetSize(rect.GetHeight(), rect.GetWidth());
#endif

  r.Move(rect.Left(), rect.Top());
  r.Intersect(r, rect);

  nuiVector vec1((float)r.Left(),(float)r.Top(),0);
  nuiVector vec2((float)r.Right(),(float)r.Bottom(),0);
  if (!mMatrixIsIdentity)
  {
    vec1 = GetMatrix() * vec1;
    vec2 = GetMatrix() * vec2;
  }

  r.Set(vec1[0], vec1[1], vec2[0], vec2[1], false);

  AddInvalidRect(r);
  mNeedRender = true;
  DebugRefreshInfo();
}

void nuiTopLevel::AddInvalidRect(const nuiRect& rRect)
{
  int count = mRedrawList.size();

  nuiRect intersect;
  int smalesti = -1;
  nuiRect smalest;
  nuiSize surface = rRect.GetSurface();
  if (surface == 0.0f)
    return;
  for (int i = 0; i<count; i++)
  {
    if (intersect.Intersect(rRect, mRedrawList[i]))
    {
      nuiRect u;
      u.Union(rRect, mRedrawList[i]);
      nuiSize temp = u.GetSurface();
      if (smalesti<0 || surface > temp)
      {
        smalesti = i;
        smalest = u;
        surface = temp;
      }
    }

  }
  if (smalesti<0) // Found no rect to blend into, let's create a new one:
    mRedrawList.push_back(rRect);
  else // Ok, this is the best intersection we could find (less surface to redraw):
    mRedrawList[smalesti] = smalest;
}

bool nuiTopLevel::SetRect(const nuiRect& rRect)
{
  nuiWidget::SetRect(rRect);
  nuiRect rect(mRect.Size());

#ifdef _UIKIT_
  if (mDrawOrigin == nuiLeft || mDrawOrigin == nuiRight)
    rect.SetSize(rect.GetHeight(), rect.GetWidth());  
  mDisplayRect=rect;
#endif

  IteratorPtr pIt;
  for (pIt = GetFirstChild(); pIt && pIt->IsValid(); GetNextChild(pIt))
  {
    nuiWidgetPtr pItem = pIt->GetWidget();
    if (pItem != mpToolTipLabel)
    {
      pItem->GetIdealRect();
      pItem->SetLayout(rect);
    }
  }
  delete pIt;

  SetToolTipRect();

  return true;
}


#ifdef _UIKIT_

bool nuiTopLevel::SetLayout(const nuiRect& rRect)
{
  bool res = false;
  nuiRect rect(rRect);
  rect.Bottom() -= mBorderBottom;
  rect.Top() += mBorderTop;
  rect.Left() += mBorderLeft;
  rect.Right() -= mBorderRight;
  
  bool SizeChanged = !rect.Size().IsEqual(mRect.Size());
  bool PositionChanged = (rect.Left() != mRect.Left()) || (rect.Top() != mRect.Top());
  mNeedSelfLayout = mNeedSelfLayout || mClippingOptims || SizeChanged;

  if (mNeedSelfLayout)
  {
    res = SetRect(rect);
  }
  else
  {
    // Is this case the widget have just been moved inside its parent. No need to re layout it, only change the rect...
    mRect = rect;
    if (mDrawOrigin == nuiLeft || mDrawOrigin == nuiRight)
      rect.SetSize(rect.GetHeight(), rect.GetWidth());
    mDisplayRect=rect;

    if (mNeedLayout)
    {
      // The children need to be re layed out (at least one of them!).
      nuiContainer::IteratorPtr pIt = GetFirstChild();
      do
      {
        nuiWidgetPtr pItem = pIt->GetWidget();
        if (pItem)
        {
          // The rect of each child doesn't change BUT we still ask for its ideal rect.
          nuiRect rect(pItem->GetBorderedRect());
          nuiRect ideal(pItem->GetIdealRect());

          if (pItem->HasUserPos()) 	 
          { 	 
            rect = ideal; 	 
          } 	 
          else if (pItem->HasUserSize())
          {
            rect.SetSize(ideal.GetWidth(), ideal.GetHeight());
          }
          else
          {
            // Set the widget to the size of the parent
          }

          pItem->SetLayout(rect);
        }
      } while (GetNextChild(pIt));
      delete pIt;

    }
  }

  //#TEST:
#ifdef NUI_CHECK_LAYOUTS
  IteratorPtr pIt;
  for (pIt = GetFirstChild(); pIt && pIt->IsValid(); GetNextChild(pIt))
  {
    nuiWidgetPtr pItem = pIt->GetWidget();
    if (pItem->IsVisible())
    {
      NGL_ASSERT(!pItem->GetNeedLayout());
    }
  }
  delete pIt;
  //#TEST end
#endif

  mNeedSelfLayout = false;
  mNeedLayout = false;
  DebugRefreshInfo();
  return res;
}

nuiRect nuiTopLevel::GetBorderedRect() const
{
  nuiRect rect = mDisplayRect;
  rect.Bottom() += mBorderBottom;
  rect.Top() -= mBorderTop;
  rect.Left() -= mBorderLeft;
  rect.Right() += mBorderRight;
  
  return rect;
}

nuiRect nuiTopLevel::GetOverDrawRect(bool LocalRect) const
{
  nuiRect r(mDisplayRect);
  if (LocalRect)
    r = r.Size();
  
  nuiSize Left = mODLeft;
  nuiSize Right = mODRight;
  nuiSize Top = mODTop;
  nuiSize Bottom = mODBottom;

  if (mpDecoration)
  {
    Left = MAX(Left, mpDecoration->GetBorder(nuiLeft));
    Right = MAX(Right, mpDecoration->GetBorder(nuiRight));
    Top = MAX(Top, mpDecoration->GetBorder(nuiTop));
    Bottom = MAX(Bottom, mpDecoration->GetBorder(nuiBottom));
  }

  r.Set(r.Left() - Left,
        r.Top() - Top,
        r.Right() + Right,
        r.Bottom() + Bottom,
        false);

  return r;
}

bool nuiTopLevel::IsInsideLocal(nuiSize X, nuiSize Y)
{
  if (!IsVisible(false))
    return false;
  if (mInteractiveOD)
    return GetOverDrawRect(false).IsInside(X, Y);
  return mDisplayRect.IsInside(X,Y);
}

#endif//_UIKIT_


bool nuiTopLevel::InitHotKeys(nglIStream* pHotKeys)
{
  nuiXML* pXML = new nuiXML(nglString::Null);
  if (pXML->Load(*pHotKeys))
  {
    NGL_OUT(pXML->Dump());
    bool success = InitHotKeys(pXML->GetChild(NUIHOTKEYS_XML_NODEID));
    delete pXML;
    
    return success;
  }
  else
  {
    NGL_OUT(_T("Couldn't load hotkeys from file\n"));
    delete pXML;
    
    return false;
  }
}

bool nuiTopLevel::InitHotKeys(nuiXMLNode* pHotKeys)
{
  nglString nodeName = pHotKeys->GetName();
  if (nodeName.Compare(NUIHOTKEYS_XML_NODEID))
  {
    NGL_OUT(_T("Not a hotkeys node\n : %ls"), nodeName.GetChars());
    return false;
  }
  
  // Parse the list of hotkeys to import
  const nuiXMLNodeList& pHotKeysList = pHotKeys->GetChildren();
  nuiXMLNodeList::const_iterator pHotKeysIt;
  
  for (pHotKeysIt = pHotKeysList.begin(); pHotKeysIt != pHotKeysList.end(); ++pHotKeysIt)
  {    
    nuiXMLNode* pNode = *pHotKeysIt;
    nglString name = pNode->GetAttribute(_T("Name"));
    nglString description = pNode->GetAttribute(_T("Description"));
    nuiKeyModifier modifiers = pNode->GetAttribute(_T("Modifiers")).GetUInt();
    bool hasPriority = pNode->GetAttribute(_T("Priority")).Compare(_T("true")) ? false : true;
    bool isOnKeyUp = pNode->GetAttribute(_T("OnKeyUp")).Compare(_T("true")) ? false : true;

    bool isHotKeyKey = pNode->GetAttribute(_T("IsHotKeyKey")).Compare(_T("true")) ? false : true;
    
    nuiHotKey* pNewHotKey;
    
    if (isHotKeyKey)
    {
      nglKeyCode trigger = pNode->GetAttribute(_T("Trigger_KeyCode")).GetUInt();
      pNewHotKey = new nuiHotKeyKey(trigger, modifiers, hasPriority, isOnKeyUp);
    }
    else
    {
      nglString trig = pNode->GetAttribute(_T("Trigger_Char"));
      nglChar trigger = (nglChar)trig.GetCInt();
      pNewHotKey = new nuiHotKeyChar(trigger, modifiers, hasPriority, isOnKeyUp);
    }
    
    pNewHotKey->SetName(name);
    pNewHotKey->SetDescription(description);
    
    SetHotKey(name, pNewHotKey);
  }
  
  return true;
}

nuiHotKey* nuiTopLevel::RegisterHotKeyKey(const nglString& rName, nglKeyCode Trigger, nuiKeyModifier Modifiers, bool Priority /*= false*/, bool FireOnKeyUp /*= false*/, const nglString& rDescription /*= nglString::Empty*/)
{
  std::map<nglString, nuiHotKey*>::const_iterator it = mHotKeys.find(rName);
  if (it == mHotKeys.end())
  {
    nuiHotKeyKey* pHotKeyKey = new nuiHotKeyKey(Trigger, Modifiers, Priority, FireOnKeyUp);
    mHotKeys[rName] = pHotKeyKey;
    pHotKeyKey->SetName(rName);
    if (rDescription.IsEmpty())
    {
      pHotKeyKey->SetDescription(rName);
    }
    else
    {
      pHotKeyKey->SetDescription(rDescription);
    }

    pHotKeyKey->Register();

    std::map<nglString, nuiSimpleEventSource<nuiWidgetActivated>*>::const_iterator it = mHotKeyEvents.find(rName);
    if (it == mHotKeyEvents.end())
      mHotKeyEvents[rName] = new nuiSimpleEventSource<nuiWidgetActivated>();

    return pHotKeyKey;
  }
  else
  {
    nuiHotKey* pHotKey = it->second;
    pHotKey->Register();

    std::map<nglString, nuiSimpleEventSource<nuiWidgetActivated>*>::const_iterator it = mHotKeyEvents.find(rName);
    if (it == mHotKeyEvents.end())
      mHotKeyEvents[rName] = new nuiSimpleEventSource<nuiWidgetActivated>();

    return pHotKey;
  }
}

nuiHotKey* nuiTopLevel::RegisterHotKeyChar(const nglString& rName, nglChar Trigger, nuiKeyModifier Modifiers, bool Priority /*= false*/, bool FireOnKeyUp /*= false*/, const nglString& rDescription /*= nglString::Empty*/)
{
  std::map<nglString, nuiHotKey*>::const_iterator it = mHotKeys.find(rName);
  if (it == mHotKeys.end())
  {
    nuiHotKeyChar* pHotKeyChar = new nuiHotKeyChar(Trigger, Modifiers, Priority, FireOnKeyUp);
    mHotKeys[rName] = pHotKeyChar;
    pHotKeyChar->SetName(rName);
    if (rDescription.IsEmpty())
    {
      pHotKeyChar->SetDescription(rName);
    }
    else
    {
      pHotKeyChar->SetDescription(rDescription);
    }
    pHotKeyChar->Register();

    std::map<nglString, nuiSimpleEventSource<nuiWidgetActivated>*>::const_iterator it = mHotKeyEvents.find(rName);
    if (it == mHotKeyEvents.end())
      mHotKeyEvents[rName] = new nuiSimpleEventSource<nuiWidgetActivated>();
    
    return pHotKeyChar;
  }
  else
  {
    nuiHotKey* pHotKey = it->second;
    pHotKey->Register();

    std::map<nglString, nuiSimpleEventSource<nuiWidgetActivated>*>::const_iterator it = mHotKeyEvents.find(rName);
    if (it == mHotKeyEvents.end())
      mHotKeyEvents[rName] = new nuiSimpleEventSource<nuiWidgetActivated>();
    
    return pHotKey;
  }
}

void nuiTopLevel::SetHotKey(const nglString& rName, nuiHotKey* pHotKey)
{  
  // search toplevel for a registered hotkey for rName
  nuiHotKey* pRegisteredHotKey = GetHotKey(rName);
  
  // if there is one, get its refCount, update the new hotkey with it, then remove the registered hotkey
  if (pRegisteredHotKey)
  {    
    //retrieve the refcount from the registered hotkey
    int refCount = pRegisteredHotKey->GetReferenceCount();
    pHotKey->SetReferenceCount(refCount);
    
    // check wether the hotkey was previously enabled
    bool enabled = pRegisteredHotKey->IsEnabled();
    pHotKey->SetEnabled(enabled);
    
    // remove the old hotkey
    delete pRegisteredHotKey;
  }
  
  // store the new hotkey
  mHotKeys[rName] = pHotKey;
}


nuiHotKey* nuiTopLevel::GetHotKey(const nglString& rName)
{
  std::map<nglString, nuiHotKey*>::const_iterator it = mHotKeys.find(rName);
  if (it != mHotKeys.end())
  {
    return it->second;
  }
  return NULL;
}

const std::map<nglString, nuiHotKey*>& nuiTopLevel::GetHotKeys() const
{
  return mHotKeys;
}

const nglString& nuiTopLevel::FindHotKeyKey(nglKeyCode Trigger, nuiKeyModifier Modifiers)
{
  std::map<nglString, nuiHotKey*>::const_iterator it;
  std::map<nglString, nuiHotKey*>::const_iterator end = mHotKeys.end();
  
  for (it = mHotKeys.begin(); it != end; it++)
  {
    nuiHotKeyKey* pKey = (nuiHotKeyKey*)it->second;
    
    if ((pKey->GetTrigger() == Trigger) && (pKey->GetModifiers() == Modifiers))
    {
      return it->first;
    }
  }
  return nglString::Empty;
}

const nglString& nuiTopLevel::FindHotKeyChar(nglChar Trigger, nuiKeyModifier Modifiers)
{
  std::map<nglString, nuiHotKey*>::const_iterator it;
  std::map<nglString, nuiHotKey*>::const_iterator end = mHotKeys.end();
  
  for (it = mHotKeys.begin(); it != end; it++)
  {
    nuiHotKeyChar* pKey = (nuiHotKeyChar*)it->second;
    
    if ((pKey->GetTrigger() == Trigger) && (pKey->GetModifiers() == Modifiers))
    {
      return it->first;
    }
  }
  return nglString::Empty;
}

void nuiTopLevel::DelHotKey( const nglString& rName)
{
  std::map<nglString, nuiHotKey*>::const_iterator it = mHotKeys.find(rName);
  if (it != mHotKeys.end())
  {
    nuiHotKey* pHotKey = it->second;
    if (pHotKey->Release())
    {
      //mHotKeys.erase(rName);
      //delete pHotKey;
    }
  }
}

void nuiTopLevel::PrintHotKeyMap(const nglString& rText)
{  
  std::map<nglString, nuiHotKey*>::const_iterator it = mHotKeys.begin();
  NGL_OUT(_T("\nShortcuts Map (%s):\n"), rText.GetStdString().c_str());
  while (it != mHotKeys.end())
  {
    nglString name = it->first;
    nuiHotKey* pHotKey = it->second;
    NGL_OUT(_T("%s -> %s\n"), name.GetStdString().c_str(), pHotKey->ShortcutToString().GetStdString().c_str());
    //NGL_OUT(_T("%ls -> %ls\n"), it->first.GetChars(), it->second->ShortcutToString().GetChars());
    ++it;
  }
  NGL_OUT(_T("\n"));
}


bool nuiTopLevel::IsTrashFull() const
{
  return !mpTrash.empty();
}

void nuiTopLevel::SetWatchedWidget(nuiWidget* pWatchedWidget)
{
  mpWatchedWidget = pWatchedWidget;
  Invalidate();
}

bool nuiTopLevel::OnMessageQueueTick(const nuiEvent& rEvent)
{
  BroadcastQueuedNotifications();
  return false;
}

//// CSS Stuff:
void nuiTopLevel::PrepareWidgetCSS(nuiWidget* pWidget, bool Recursive, uint32 MatchersTag)
{
  mCSSWidgets[pWidget] |= MatchersTag;

  if (!Recursive)
    return;
  
  nuiContainer* pContainer = dynamic_cast<nuiContainer*>(pWidget);
  if (pContainer)
  {
    IteratorPtr pIt;
    for (pIt = pContainer->GetFirstChild(); pIt && pIt->IsValid(); pContainer->GetNextChild(pIt))
    {
      nuiWidgetPtr pItem = pIt->GetWidget();
      PrepareWidgetCSS(pItem, Recursive, MatchersTag);
    }
    delete pIt;
  }
}

void nuiTopLevel::ApplyWidgetCSS(nuiWidget* pWidget, bool Recursive, uint32 MatchersTag)
{
  if (!mpCSS)
    return;
  
  mpCSS->ApplyRules(pWidget, MatchersTag);

  if (Recursive)
  {
    nuiContainer* pContainer = dynamic_cast<nuiContainer*>(pWidget);
    if (pContainer)
    {
      IteratorPtr pIt;
      for (pIt = pContainer->GetFirstChild(); pIt && pIt->IsValid(); pContainer->GetNextChild(pIt))
      {
        nuiWidgetPtr pItem = pIt->GetWidget();
        ApplyWidgetCSS(pItem, Recursive, MatchersTag);
      }
      delete pIt;
    }
  }
}

void nuiTopLevel::UpdateWidgetsCSS()
{
  std::map<nuiWidgetPtr, uint32>::iterator it = mCSSWidgets.begin();
  std::map<nuiWidgetPtr, uint32>::iterator end = mCSSWidgets.end();
  
  while (it != end)
  {
    nuiWidget* pWidget = it->first;
    uint32 MatchersTag = it->second;
    
    ApplyWidgetCSS(pWidget, false, MatchersTag);
    
    ++it;
  }
  
  mCSSWidgets.clear();
}

void nuiTopLevel::SetCSS(nuiCSS* pCSS)
{
  if (mpCSS != pCSS)
    delete mpCSS;
  mpCSS = pCSS;
  
  if (mpCSS)
  {
    ResetCSSPass();
    ApplyWidgetCSS(this, true, NUI_WIDGET_MATCHTAG_ALL);
  }
}

nuiCSS* nuiTopLevel::GetCSS() const
{
  return mpCSS;
}

void nuiTopLevel::EnterModalState()
{
}

void nuiTopLevel::ExitModalState()
{
}

void nuiTopLevel::SetTabForward(nuiWidget* pSource, nuiWidget* pDestination, bool AutoReverse)
{
  // First check f there is already an entry in the table for this source
  std::map<nuiWidgetPtr, nuiWidgetPtr>::iterator it = mTabForward.find(pSource);
  
  // if this is the case then remove the back pointer
  if (it != mTabForward.end())
  {
    if (!pDestination)
    {
      mTabForward.erase(it);
      return;
    }
    
    if (it->second != pDestination)
      mTabForwardRev[pDestination].erase(pSource);
    else
      return; // Nothing to do
  }

  if (!pDestination)
    return;
  
  // Add the relation to the table
  mTabForward[pSource] = pDestination;

  // Add the relation to the reverse table (for quick removal when the pointed widget will die)
  mTabForwardRev[pDestination].insert(pSource);
  
  // If needed, added the bacward relation too (so that Tab and then shift-Tab goes back to the original source)
  if (AutoReverse)
  {
    SetTabBackward(pDestination, pSource, false);
  }
  
}

void nuiTopLevel::SetTabBackward(nuiWidget* pSource, nuiWidget* pDestination, bool AutoReverse)
{
  // First check f there is already an entry in the table for this source
  std::map<nuiWidgetPtr, nuiWidgetPtr>::iterator it = mTabBackward.find(pSource);
  
  // if this is the case then remove the back pointer
  if (it != mTabBackward.end())
  {
    if (!pDestination)
    {
      mTabBackward.erase(it);
      return;
    }
    
    if (it->second != pDestination)
      mTabBackwardRev[pDestination].erase(pSource);
    else
      return; // Nothing to do
  }

  if (!pDestination)
    return;
  
  // Add the relation to the table
  mTabBackward[pSource] = pDestination;
  
  // Add the relation to the reverse table (for quick removal when the pointed widget will die)
  mTabBackwardRev[pDestination].insert(pSource);
  
  // If needed, added the bacward relation too (so that Tab and then shift-Tab goes back to the original source)
  if (AutoReverse)
  {
    SetTabForward(pDestination, pSource, false);
  }
  
}

nuiWidget* nuiTopLevel::GetTabForward(nuiWidget* pSource) const
{
  std::map<nuiWidgetPtr, nuiWidgetPtr>::const_iterator it = mTabForward.find(pSource);
  if (it != mTabForward.end())
    return it->second;
  return NULL;
}

nuiWidget* nuiTopLevel::GetTabBackward(nuiWidget* pSource) const
{
  std::map<nuiWidgetPtr, nuiWidgetPtr>::const_iterator it = mTabBackward.find(pSource);
  if (it != mTabBackward.end())
    return it->second;
  return NULL;
}

void nuiTopLevel::GetTabForwardSources(nuiWidget* pDestination, std::set<nuiWidgetPtr>& rSources) const
{
  std::map<nuiWidgetPtr, std::set<nuiWidgetPtr> >::const_iterator it = mTabForwardRev.find(pDestination);
  if (it != mTabForwardRev.end())
  {
    rSources = it->second;
    return;
  }
  rSources.clear();
}

void nuiTopLevel::GetTabBackwardSources(nuiWidget* pDestination, std::set<nuiWidgetPtr>& rSources) const
{
  std::map<nuiWidgetPtr, std::set<nuiWidgetPtr> >::const_iterator it = mTabBackwardRev.find(pDestination);
  if (it != mTabBackwardRev.end())
  {
    rSources = it->second;
    return;
  }
  rSources.clear();
}

