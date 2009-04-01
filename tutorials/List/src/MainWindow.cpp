/*
  NUI3 - C++ cross-platform GUI framework for OpenGL based applications
  Copyright (C) 2002-2003 Sebastien Metrot

  licence: see nui3/LICENCE.TXT
*/

#include "nui.h"
#include "MainWindow.h"
#include "Application.h"
#include "nuiCSS.h"
#include "nuiVBox.h"
#include "nuiBackgroundPane.h"

/*
 * MainWindow
 */

MainWindow::MainWindow(const nglContextInfo& rContextInfo, const nglWindowInfo& rInfo, bool ShowFPS, const nglContext* pShared )
  : nuiMainWindow(rContextInfo, rInfo, pShared, nglPath(ePathCurrent)), mEventSink(this)
{
#ifdef _DEBUG_
  SetDebugMode(true);
#endif
  
  LoadCSS(_T("rsrc:/css/main.css"));
}

MainWindow::~MainWindow()
{
}

void MainWindow::OnCreation()
{
  nuiVBox* pBox = new nuiVBox(0);
  pBox->SetPosition(nuiCenter);
  AddChild(pBox);
  
  // output label
  nuiBackgroundPane* pPane = new nuiBackgroundPane(eInnerBackground);
  pPane->SetUserSize(300, 30);
  pBox->AddCell(pPane);
  
  mpOutput = new nuiLabel();
  pPane->AddChild(mpOutput);
  
  // comment
  pBox->AddCell(_T("- Drag the elements to move them in the list\n- Click to select\n- Double click to activate"));
  

  // input list
  pPane = new nuiBackgroundPane();
  pPane->SetUserSize(300,380);
  pBox->AddCell(pPane);
  
  nuiScrollView* pScroll = new nuiScrollView();
  pPane->AddChild(pScroll);
  
  mpList = new nuiList();
  pScroll->AddChild(mpList);
  
  for (uint32 i = 0; i < 16; i++)
  {
    nglString str;
    str.CFormat(_T("Item %d"), i);
    nuiLabel* pItem = new nuiLabel(str);
    
    // bind a user data to the item's widget
    pItem->SetToken(new nuiToken<uint32>(i));
    
    mpList->AddChild(pItem);
  }
  
  mpList->SetCanMoveItems(true);
  

  mEventSink.Connect(mpList->SelectionChanged, &MainWindow::OnItemSelected);
  mEventSink.Connect(mpList->Activated, &MainWindow::OnItemActivated);
  
}


bool MainWindow::OnItemSelected(const nuiEvent& rEvent)
{
  nuiWidget* pSelectedWidget = mpList->GetSelected();
  uint32 token;
  nuiGetTokenValue<uint32>(pSelectedWidget->GetToken(), token);
  
  // or, use the shortcut mpList->GetSelectedToken :)
  
  nglString message;
  message.Format(_T("selected item num %d"), token);
  mpOutput->SetText(message);
  
  return false;
}


bool MainWindow::OnItemActivated(const nuiEvent& rEvent)
{
  uint32 token;
  nuiGetTokenValue<uint32>(mpList->GetSelectedToken(), token);
  
  nglString message;
  message.Format(_T("activated item num %d"), token);
  mpOutput->SetText(message);

  return true;  
}



void MainWindow::OnClose()
{
  if (GetNGLWindow()->IsInModalState())
    return;
  
  
  App->Quit();
}


bool MainWindow::LoadCSS(const nglPath& rPath)
{
  nglIStream* pF = rPath.OpenRead();
  if (!pF)
  {
    NGL_OUT(_T("Unable to open CSS source file '%ls'\n"), rPath.GetChars());
    return false;
  }
  
  nuiCSS* pCSS = new nuiCSS();
  bool res = pCSS->Load(*pF, rPath);
  delete pF;
  
  if (res)
  {
    nuiMainWindow::SetCSS(pCSS);
    return true;
  }
  
  NGL_OUT(_T("%ls\n"), pCSS->GetErrorString().GetChars());
  
  delete pCSS;
  return false;
}