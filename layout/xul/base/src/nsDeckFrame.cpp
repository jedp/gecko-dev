/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsDeckFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleChangeList.h"
#include "nsCSSRendering.h"
#include "nsIViewManager.h"
#include "nsBoxLayoutState.h"
#include "nsStackLayout.h"
#include "nsWidgetsCID.h"
#include "nsHTMLContainerFrame.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

nsresult
NS_NewDeckFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDeckFrame* it = new (aPresShell) nsDeckFrame(aPresShell, aLayoutManager);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewDeckFrame


nsDeckFrame::nsDeckFrame(nsIPresShell* aPresShell, nsIBoxLayout* aLayoutManager):nsBoxFrame(aPresShell),mIndex(0)
{
     // if no layout manager specified us the static sprocket layout
  nsCOMPtr<nsIBoxLayout> layout = aLayoutManager;

  if (layout == nsnull) {
    NS_NewStackLayout(aPresShell, layout);
  }

  SetLayoutManager(layout);
}

/**
 * Hack for deck who requires that all its children has widgets
 */
NS_IMETHODIMP
nsDeckFrame::ChildrenMustHaveWidgets(PRBool& aMust)
{
  aMust = PR_TRUE;
  return NS_OK;
}

nsresult
nsDeckFrame::CreateWidget(nsIPresContext* aPresContext, nsIBox* aBox)
{
  nsresult rv = NS_OK;

  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);

  nsIView* view = nsnull;
  frame->GetView(aPresContext, &view);
  
  if (!view) {
     nsCOMPtr<nsIStyleContext> context;
     frame->GetStyleContext(getter_AddRefs(context));
     nsHTMLContainerFrame::CreateViewForFrame(aPresContext,frame,context,nsnull,PR_TRUE); 
     frame->GetView(aPresContext, &view);
  }

  nsIWidget* widget;
  view->GetWidget(widget);

  if (!widget)
     rv = view->CreateWidget(kWidgetCID);

  return rv;
}

nsresult
nsDeckFrame::CreateWidgets(nsIPresContext* aPresContext)
{
  // create a widget for each child.
  nsIBox* child = nsnull;
  GetChildBox(&child);
  while(child)
  {
    CreateWidget(aPresContext, child);
    child->GetNextBox(&child);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDeckFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  //CreateWidgets(aPresContext);
  return rv;
}

NS_IMETHODIMP
nsDeckFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  nsresult rv = nsBoxFrame::AppendFrames(aPresContext, aPresShell, aListName, aFrameList);
  //CreateWidgets(aPresContext);
  return rv;
}

NS_IMETHODIMP
nsDeckFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  nsresult rv = nsBoxFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame, aFrameList);
  //CreateWidgets(aPresContext);
  return rv;
}

NS_IMETHODIMP
nsDeckFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);


   // if the index changed hide the old element and make the now element visible
  if (aAttribute == nsHTMLAtoms::index) {
    IndexChanged(aPresContext);
  }

  return rv;
}

NS_IMETHODIMP
nsDeckFrame::Init(nsIPresContext*  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aStyleContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent,
                                            aParent, aStyleContext,
                                            aPrevInFlow);

  mIndex = GetSelectedIndex();

  return rv;
}

void
nsDeckFrame::HideBox(nsIPresContext* aPresContext, nsIBox* aBox)
{
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);

  nsIView* view = nsnull;
  frame->GetView(aPresContext, &view);

  if (view) {
    nsCOMPtr<nsIViewManager> viewManager;
    view->GetViewManager(*getter_AddRefs(viewManager));
    viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
    viewManager->ResizeView(view, 0, 0);
  }
}

void
nsDeckFrame::ShowBox(nsIPresContext* aPresContext, nsIBox* aBox)
{
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);

  nsRect rect;
  frame->GetRect(rect);
  nsIView* view = nsnull;
  frame->GetView(aPresContext, &view);
  if (view) {
    nsCOMPtr<nsIViewManager> viewManager;
    view->GetViewManager(*getter_AddRefs(viewManager));
    viewManager->ResizeView(view, rect.width, rect.height);
    viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
  }
}

void
nsDeckFrame::IndexChanged(nsIPresContext* aPresContext)
{
  //did the index change?
  PRInt32 index = GetSelectedIndex();
  if (index == mIndex)
    return;

  // redraw
  nsBoxLayoutState state(aPresContext);
  Redraw(state);

  // hide the currently showing box
  nsIBox* currentBox = GetBoxAt(mIndex);
  if (currentBox) // only hide if it exists
     HideBox(aPresContext, currentBox);

  // show the new box
  nsIBox* newBox = GetBoxAt(index);
  if (newBox) // only show if it exists
     ShowBox(aPresContext, newBox);

  mIndex = index;
}

PRInt32
nsDeckFrame::GetSelectedIndex()
{
 // default index is 0
  PRInt32 index = 0;

  // get the index attribute
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::index, value))
  {
    PRInt32 error;

    // convert it to an integer
    index = value.ToInteger(&error);
  }

  return index;
}

nsIBox* 
nsDeckFrame::GetSelectedBox()
{
 // ok we want to paint only the child that as at the given index
  PRInt32 index = GetSelectedIndex();
 
  // get the child at that index. 
  return GetBoxAt(index); 
}


NS_IMETHODIMP
nsDeckFrame::Paint(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
  // if a tab is hidden all its children are too.
 	const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->mVisible)
		return NS_OK;

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    if (disp->IsVisible() && mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color = (const nsStyleColor*)
        mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleBorder* border = (const nsStyleBorder*)
        mStyleContext->GetStyleData(eStyleStruct_Border);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *border, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *border, mStyleContext, skipSides);
    }
  }

  // only paint the seleced box
  nsIBox* box = GetSelectedBox();
  if (box) {
    nsIFrame* frame = nsnull;
    box->GetFrame(&frame);

    if (frame != nsnull)
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, frame, aWhichLayer);
  }

  return NS_OK;

}


NS_IMETHODIMP  nsDeckFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                             const nsPoint& aPoint, 
                                             nsFramePaintLayer aWhichLayer,    
                                             nsIFrame**     aFrame)
{

  if ((aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND))
    return NS_ERROR_FAILURE;

  // if it is not inside us fail
  if (!mRect.Contains(aPoint)) {
      return NS_ERROR_FAILURE;
  }

  // if its not in our child just return us.
  *aFrame = this;

  // get the selected frame and see if the point is in it.
  nsIBox* selectedBox = GetSelectedBox();
  if (selectedBox) {
    nsIFrame* selectedFrame = nsnull;
    selectedBox->GetFrame(&selectedFrame);

    nsPoint tmp;
    tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);

    return selectedFrame->GetFrameForPoint(aPresContext, tmp, aWhichLayer, aFrame);
  }
    
  return NS_OK;
}


NS_IMETHODIMP
nsDeckFrame::DoLayout(nsBoxLayoutState& aState)
{
  // make sure we tweek the state so it does not resize our children. We will do that.
  PRUint32 oldFlags = 0;
  aState.GetLayoutFlags(oldFlags);
  aState.SetLayoutFlags(NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_VISIBILITY);

  // do a normal layout
  nsresult rv = nsBoxFrame::DoLayout(aState);

  // run though each child. Hide all but the selected one
  nsIBox* box = nsnull;
  GetChildBox(&box);

  nscoord count = 0;
  while (box) 
  {
    // make collapsed children not show up
    if (count == mIndex) 
       ShowBox(aState.GetPresContext(), box);
    else
       HideBox(aState.GetPresContext(), box);

    nsresult rv2 = box->GetNextBox(&box);
    NS_ASSERTION(rv2 == NS_OK,"failed to get next child");
    count++;
  }

  aState.SetLayoutFlags(oldFlags);

  return rv;
}

