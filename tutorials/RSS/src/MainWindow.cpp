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
#include "nuiHBox.h"

#include "nuiRSSView.h"

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
  //nglString url(_T("http://twitter.com/statuses/user_timeline/21746237.rss"));
  nglString url(_T("feed://feeds.macbidouille.com/macbidouille/"));
  //nglString url(_T("feed://www.lemonde.fr/rss/une.xml"));
  //nglString url(_T("feed://newsrss.bbc.co.uk/rss/newsonline_world_edition/front_page/rss.xml"));
  
  
  // a vertical box for page layout
  nuiVBox* pLayoutBox = new nuiVBox(0);
  pLayoutBox->SetExpand(nuiExpandShrinkAndGrow);
  AddChild(pLayoutBox);

  nuiHBox* pHLayoutBox = new nuiHBox(0);
  pHLayoutBox->SetExpand(nuiExpandShrinkAndGrow);
  pLayoutBox->AddCell(pHLayoutBox);
//  pLayoutBox->SetCellExpand(pLayoutBox->GetNbCells()-1, nuiExpandShrinkAndGrow);

  // image in the first box's cell
  nuiImage* pImg = new nuiImage();
  pImg->SetObjectName(_T("MyImage"));
  pImg->SetPosition(nuiCenter);
  pHLayoutBox->AddCell(pImg);
  pHLayoutBox->SetCellExpand(pHLayoutBox->GetNbCells()-1, nuiExpandFixed);

  // button in the second cell : we use the default decoration for this button, but you could use the css to assign your own decoration
  mpInput = new nuiEditLine(url);
  mpInput->SetPosition(nuiCenter);
  mpInput->SetObjectName(_T("RSSURL"));
  mEventSink.Connect(mpInput->Activated, &MainWindow::OnButtonClick);
  pHLayoutBox->AddCell(mpInput);
  pHLayoutBox->SetCellExpand(pHLayoutBox->GetNbCells()-1, nuiExpandShrinkAndGrow);
  
  // button in the second cell : we use the default decoration for this button, but you could use the css to assign your own decoration
  nuiButton* pButton = new nuiButton();
  pButton->SetPosition(nuiCenter);
  pHLayoutBox->AddCell(pButton);
  pHLayoutBox->SetCellExpand(pHLayoutBox->GetNbCells()-1, nuiExpandFixed);
  
  // click event on button
  mEventSink.Connect(pButton->Activated, &MainWindow::OnButtonClick);
  
  // label with border in the button (put the label string in the button's constructor if you don't need borders)
  nuiLabel* pButtonLabel = new nuiLabel(_T("Update RSS!"));
  pButtonLabel->SetPosition(nuiCenter);
  pButtonLabel->SetBorder(8,8);
  pButton->AddChild(pButtonLabel);

  mpRSSView = new nuiRSSView(url);
  
  nuiScrollView* pScroll = new nuiScrollView(false, true);
  pScroll->AddChild(mpRSSView);
  pLayoutBox->AddCell(pScroll);
  pLayoutBox->SetCellExpand(pLayoutBox->GetNbCells()-1, nuiExpandShrinkAndGrow);
}



void MainWindow::OnButtonClick(const nuiEvent& rEvent)
{
  nglString message;
  double currentTime = nglTime();
  message.Format(_T("click time: %.2f"), currentTime);
  mpRSSView->SetURL(mpInput->GetText());
  mpRSSView->ForceUpdate();
  
  // the event is caught and not broadcasted
  rEvent.Cancel();
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
