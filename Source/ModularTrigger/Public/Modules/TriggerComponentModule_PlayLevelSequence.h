
#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_PlayLevelSequence.generated.h"

class ALevelSequenceActor;

/** What the module does when the event is an end overlap rather than a begin overlap. */
UENUM()
enum class ETriggerSequenceEndOverlapAction : uint8
{
	/** Leaving the trigger does nothing; the sequence runs to its end. */
	Ignore,
	/** Stop playback, honouring the sequence's "When Finished" section rules. */
	Stop,
	/** Freeze playback where it is, so a later begin overlap can resume it. */
	Pause,
};

/**
 * Plays a level sequence through an ALevelSequenceActor placed in the level.
 *
 * The actor is the unit of authoring, not the sequence asset: it carries the binding overrides and the
 * playback settings the cinematic was set up with, which a player spawned from the bare asset would not
 * have. Playback settings that already exist on that actor — play rate, "When Finished", loop count for
 * its own auto-play — are left alone; this module only decides when playback starts and stops.
 */
UCLASS(DisplayName = "Play Level Sequence")
class MODULARTRIGGER_API UTriggerComponentModule_PlayLevelSequence : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()

public:

	/**
	 * The sequence actor to drive. Per-placement, so it is instance-only: a sequence actor reference on a
	 * Blueprint default would be the same actor for every placed copy.
	 *
	 * Must live in the same level as the trigger, or in one guaranteed to be loaded alongside it — this is a
	 * hard reference, so a streamed-out target resolves to null and the module does nothing.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Trigger Module")
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	/**
	 * Play backwards, from the end of the playback range towards the start.
	 *
	 * Independent of bLoop: direction and loop count are separate state on the player, and the engine's
	 * loop handling wraps at whichever end the direction makes the far one, so a reversed loop works.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module")
	uint8 bPlayReversed : 1 = false;

	/** Restart the sequence each time it reaches the far end of its playback range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module")
	uint8 bLoop : 1 = false;
	
	/**
	 * Jump to the start of the playback range before playing, rather than resuming from wherever the cursor
	 * was left. Turn this off to make a paused sequence continue where it stopped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module")
	uint8 bRestartFromStart : 1 = true;

	/**
	 * Leave a sequence that is already playing alone. Off means a second overlap interrupts it, which with
	 * bRestartFromStart set restarts a cinematic from the beginning mid-shot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module")
	uint8 bIgnoreIfAlreadyPlaying : 1 = true;

	/** Times to loop. -1 loops until something stops it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module",
		meta = (EditCondition = "bLoop", ClampMin = "-1"))
	int32 NumLoops = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module")
	ETriggerSequenceEndOverlapAction OnEndOverlap = ETriggerSequenceEndOverlapAction::Ignore;

#if WITH_EDITOR
	//~ Begin UHierarchicalTriggerComponentModule interface
	virtual void DescribeVisualization(FTriggerVisualizationBuilder& Builder) const override;
	//~ End UHierarchicalTriggerComponentModule interface
#endif

protected:

	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) override;
};
