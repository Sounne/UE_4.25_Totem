// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackRecorders/MovieScene3DAttachTrackRecorder.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Modules/ModuleManager.h"
#include "SequenceRecorderUtils.h"
#include "MovieScene.h"

bool FMovieScene3DAttachTrackRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	return InObjectToRecord->IsA<AActor>();
}

UMovieSceneTrackRecorder* FMovieScene3DAttachTrackRecorderFactory::CreateTrackRecorderForObject() const
{
	return NewObject<UMovieScene3DAttachTrackRecorder>();
}

void UMovieScene3DAttachTrackRecorder::RecordSampleImpl(const FQualifiedFrameTime& CurrentTime)
{
	AActor* ActorToRecord = Cast<AActor>(ObjectToRecord.Get());
	if (ActorToRecord)
	{
		if (MovieSceneSection.IsValid())
		{
			FFrameRate TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
			FFrameNumber CurrentFrame = CurrentTime.ConvertTo(TickResolution).FloorToFrame();

			MovieSceneSection->SetEndFrame(CurrentFrame);
		}

		// get attachment and check if the actor we are attached to is being recorded
		FName SocketName;
		FName ComponentName;
		AActor* AttachedToActor = SequenceRecorderUtils::GetAttachment(ActorToRecord, SocketName, ComponentName);

		if (AttachedToActor && OwningTakeRecorderSource->IsOtherActorBeingRecorded(AttachedToActor))
		{
			Guid = OwningTakeRecorderSource->GetRecordedActorGuid(AttachedToActor);

			// create the Section if we haven't already
			if (!AttachTrack.IsValid())
			{
				AttachTrack = MovieScene->AddTrack<UMovieScene3DAttachTrack>(ObjectGuid);
			}

			// check if we need a section or if the actor we are attached to has changed
			if (!MovieSceneSection.IsValid() || AttachedToActor != ActorAttachedTo.Get())
			{
				MovieSceneSection = Cast<UMovieScene3DAttachSection>(AttachTrack->CreateNewSection());
				MovieSceneSection->AttachSocketName = SocketName;
				MovieSceneSection->AttachComponentName = ComponentName;

				// Newly created attach sections should match the parent movie scene. This is effectively an infinite attach section, 
				// but clamped to the bounds of the parent movie scene
				MovieSceneSection->TimecodeSource = MovieScene->TimecodeSource;
				MovieSceneSection->SetRange(MovieScene->GetPlaybackRange());
			}

			ActorAttachedTo = AttachedToActor;
		}
		else
		{
			// no attachment, so end the section recording if we have any
			MovieSceneSection = nullptr;
		}
	}
}

void UMovieScene3DAttachTrackRecorder::FinalizeTrackImpl()
{
	AActor* ActorToRecord = Cast<AActor>(ObjectToRecord.Get());
	if (ActorToRecord)
	{
		FName SocketName;
		FName ComponentName;
		AActor* AttachedToActor = SequenceRecorderUtils::GetAttachment(ActorToRecord, SocketName, ComponentName);
		//note this actor may no longer exist BUT we need to do this in finalize since the compilation only happens there.
		//fix would be to have a way to get the sequence id differently then doing the compilation above
		//or have another way to do cleanup
		if (MovieSceneSection.IsValid() && AttachedToActor)
		{
			if (!Guid.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Could not find binding to attach (%s) to its parent (%s), perhaps (%s) was not recorded?"), *ActorToRecord->GetActorLabel(), *AttachedToActor->GetActorLabel(), *AttachedToActor->GetActorLabel());
			}

			FMovieSceneSequenceID SequenceID;
			SequenceID = OwningTakeRecorderSource->GetLevelSequenceID(AttachedToActor);
			MovieSceneSection->SetConstraintId(Guid, SequenceID);
		}
	}
}
