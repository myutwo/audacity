/**********************************************************************

  Audacity: A Digital Audio Editor

  Repeat.cpp

  Dominic Mazzoni
  Vaughan Johnson

*******************************************************************//**

\class EffectRepeat
\brief An Effect.

*//****************************************************************//**

\class RepeatDialog
\brief Dialog used with EffectRepeat

*//*******************************************************************/


#include "../Audacity.h"


#include <math.h>

#include <wx/intl.h>

#include "../LabelTrack.h"
#include "../ShuttleGui.h"
#include "../WaveTrack.h"
#include "../widgets/NumericTextCtrl.h"
#include "../widgets/valnum.h"

#include "Repeat.h"

BEGIN_EVENT_TABLE(EffectRepeat, wxEvtHandler)
   EVT_TEXT(wxID_ANY, EffectRepeat::OnRepeatTextChange)
END_EVENT_TABLE()

EffectRepeat::EffectRepeat()
{
   repeatCount = 10;
}

EffectRepeat::~EffectRepeat()
{
}

// IdentInterface implementation

wxString EffectRepeat::GetSymbol()
{
   return REPEAT_PLUGIN_SYMBOL;
}

wxString EffectRepeat::GetDescription()
{
   return wxTRANSLATE("Repeats the selection the specified number of times");
}

// EffectIdentInterface implementation

EffectType EffectRepeat::GetType()
{
   return EffectTypeProcess;
}

// EffectClientInterface implementation

bool EffectRepeat::GetAutomationParameters(EffectAutomationParameters & parms)
{
   parms.Write(wxT("Count"), repeatCount);

   return true;
}

bool EffectRepeat::SetAutomationParameters(EffectAutomationParameters & parms)
{
   int count;

   parms.Read(wxT("Count"), &count, 10);

   if (count < 0 || count > 2147483647 / mProjectRate)
   {
      return false;
   }

   repeatCount = count;

   return true;
}

// Effect implementation

bool EffectRepeat::Process()
{
   // Set up mOutputTracks.
   // This effect needs Track::All for sync-lock grouping.
   CopyInputTracks(Track::All);

   int nTrack = 0;
   bool bGoodResult = true;
   double maxDestLen = 0.0; // used to change selection to generated bit

   TrackListIterator iter(mOutputTracks);

   for (Track *t = iter.First(); t && bGoodResult; t = iter.Next())
   {
      if (t->GetKind() == Track::Label)
      {
         if (t->GetSelected() || t->IsSyncLockSelected())
         {
            LabelTrack* track = (LabelTrack*)t;

            if (!track->Repeat(mT0, mT1, repeatCount))
            {
               bGoodResult = false;
               break;
            }
         }
      }
      else if (t->GetKind() == Track::Wave && t->GetSelected())
      {
         WaveTrack* track = (WaveTrack*)t;

         sampleCount start = track->TimeToLongSamples(mT0);
         sampleCount end = track->TimeToLongSamples(mT1);
         sampleCount len = (sampleCount)(end - start);
         double tLen = track->LongSamplesToTime(len);
         double tc = mT0 + tLen;

         if (len <= 0)
         {
            continue;
         }

         Track *dest;
         track->Copy(mT0, mT1, &dest);
         for(int j=0; j<repeatCount; j++)
         {
            if (!track->Paste(tc, dest) ||
                  TrackProgress(nTrack, j / repeatCount)) // TrackProgress returns true on Cancel.
            {
               bGoodResult = false;
               break;
            }
            tc += tLen;
         }
         if (tc > maxDestLen)
            maxDestLen = tc;
         delete dest;
         nTrack++;
      }
      else if (t->IsSyncLockSelected())
      {
         t->SyncLockAdjust(mT1, mT1 + (mT1 - mT0) * repeatCount);
      }
   }

   if (bGoodResult)
   {
      // Select the new bits + original bit
      mT1 = maxDestLen;
   }

   ReplaceProcessedTracks(bGoodResult);
   return bGoodResult;
}

void EffectRepeat::PopulateOrExchange(ShuttleGui & S)
{
   S.StartHorizontalLay(wxCENTER, false);
   {
      IntegerValidator<int> vldRepeatCount(&repeatCount);
      vldRepeatCount.SetRange(1, 2147483647 / mProjectRate);
      mRepeatCount = S.AddTextBox(_("Number of times to repeat:"), wxT(""), 12);
      mRepeatCount->SetValidator(vldRepeatCount);
   }
   S.EndHorizontalLay();

   S.StartHorizontalLay(wxCENTER, true);
   {
      mTotalTime = S.AddVariableText(_("New selection length: dd:hh:mm:ss"));
   }
   S.EndHorizontalLay();
}

bool EffectRepeat::TransferDataToWindow()
{
   mRepeatCount->ChangeValue(wxString::Format(wxT("%d"), repeatCount));

   DisplayNewTime();

   return true;
}

bool EffectRepeat::TransferDataFromWindow()
{
   if (!mUIParent->Validate())
   {
      return false;
   }

   long l;

   mRepeatCount->GetValue().ToLong(&l);

   repeatCount = (int) l;

   return true;
}

void EffectRepeat::DisplayNewTime()
{
   TransferDataFromWindow();

   NumericConverter nc(NumericConverter::TIME,
                       _("hh:mm:ss"),
                       (mT1 - mT0) * (repeatCount + 1),
                       mProjectRate);

   wxString str = _("New selection length: ") + nc.GetString();

   mTotalTime->SetLabel(str);
   mTotalTime->SetName(str); // fix for bug 577 (NVDA/Narrator screen readers do not read static text in dialogs)
}

void EffectRepeat::OnRepeatTextChange(wxCommandEvent & WXUNUSED(evt))
{
   DisplayNewTime();
}
