
#include "Modules/TriggerComponentModule_PlayLevelSequence.h"

#include "GameFramework/Actor.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

#if WITH_EDITOR
#include "TriggerModuleVisualization.h"
#endif

#define LOCTEXT_NAMESPACE "TriggerComponentModule_PlayLevelSequence"

#if WITH_EDITOR
namespace
{
	/** Colour of a resolved sequence actor, and of an empty reference. */
	const FLinearColor SequenceColor(0.35f, 0.55f, 0.95f);
	const FLinearColor EmptySlotColor(0.90f, 0.25f, 0.20f);

	/** Radius of the marker drawn on the sequence actor. */
	constexpr double SequenceMarkerRadius = 48.0;
}
#endif

void UTriggerComponentModule_PlayLevelSequence::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	if (!IsValid(SequenceActor))
	{
		return;
	}

	// Null until the actor has initialized its player, which happens on its own BeginPlay — so an event
	// generated during the initial overlap sweep can legitimately arrive before there is anything to play.
	ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
	if (!Player)
	{
		return;
	}

	if (TriggerExecutionContext.IsEndOverlap())
	{
		switch (OnEndOverlap)
		{
		case ETriggerSequenceEndOverlapAction::Stop:
			Player->Stop();
			break;

		case ETriggerSequenceEndOverlapAction::Pause:
			Player->Pause();
			break;

		case ETriggerSequenceEndOverlapAction::Ignore:
		default:
			break;
		}
		return;
	}

	if (bIgnoreIfAlreadyPlaying && Player->IsPlaying())
	{
		return;
	}

	if (bRestartFromStart)
	{
		// The start of the playback range, which is not necessarily time zero.
		Player->SetPlaybackPosition(
			FMovieSceneSequencePlaybackParams(Player->GetStartTime().Time, EUpdatePositionMethod::Jump));
	}

	// Direction first, because this is the call that actually starts playback — PlayInternal does nothing
	// once the player is playing — so the very first evaluation already runs the right way round.
	if (bPlayReversed)
	{
		Player->PlayReverse();
	}
	else
	{
		Player->Play();
	}

	// PlayLooping only writes PlaybackSettings.LoopCount, which is not read until the cursor reaches the far
	// end of the range, so setting it after the start is fine. Called unconditionally: 0 is the engine's "no
	// loops", and without it a count left behind by an earlier looping play would still be in effect.
	Player->PlayLooping(bLoop ? NumLoops : 0);
}

#if WITH_EDITOR
void UTriggerComponentModule_PlayLevelSequence::DescribeVisualization(FTriggerVisualizationBuilder& Builder) const
{
	const FVector Origin = Builder.GetOrigin();

	if (!IsValid(SequenceActor))
	{
		Builder.AddText(Origin, LOCTEXT("EmptySequenceActor", "Play Level Sequence: no sequence actor"), EmptySlotColor);
		return;
	}

	const FVector SequenceLocation = SequenceActor->GetActorLocation();

	Builder.AddArrow(Origin, SequenceLocation)
		.SetColor(SequenceColor)
		.SetClickTarget(SequenceActor);

	Builder.AddSphere(SequenceLocation, SequenceMarkerRadius)
		.SetColor(SequenceColor)
		.SetLabel(FText::FromString(SequenceActor->GetActorNameOrLabel()))
		.SetClickTarget(SequenceActor);
}
#endif

#undef LOCTEXT_NAMESPACE
