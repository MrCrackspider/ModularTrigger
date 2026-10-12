
#include "TriggerModuleVisualization.h"

#if WITH_EDITOR

#include "GameFramework/Actor.h"
#include "HierarchicalTriggerComponent.h"

FVector FTriggerVisualizationPrimitive::GetLabelAnchor() const
{
	switch (Shape)
	{
	case ETriggerVisualizationShape::Line:
	case ETriggerVisualizationShape::Arrow:
		return (Transform.GetLocation() + EndLocation) * 0.5;
	default:
		return Transform.GetLocation();
	}
}

FTriggerVisualizationHandle::FTriggerVisualizationHandle(FTriggerVisualizationBuilder& InBuilder, int32 InIndex)
	: Builder(InBuilder)
	, Index(InIndex)
{
}

FTriggerVisualizationPrimitive& FTriggerVisualizationHandle::Get() const
{
	check(Builder.Primitives.IsValidIndex(Index));
	return Builder.Primitives[Index];
}

FTriggerVisualizationHandle& FTriggerVisualizationHandle::SetColor(const FLinearColor& InColor)
{
	Get().Color = InColor;
	return *this;
}

FTriggerVisualizationHandle& FTriggerVisualizationHandle::SetThickness(float InThickness)
{
	Get().Thickness = InThickness;
	return *this;
}

FTriggerVisualizationHandle& FTriggerVisualizationHandle::SetLabel(const FText& InLabel)
{
	Get().Label = InLabel;
	return *this;
}

FTriggerVisualizationHandle& FTriggerVisualizationHandle::SetClickTarget(AActor* InActor)
{
	Get().ClickTarget = InActor;
	return *this;
}

FTriggerVisualizationBuilder::FTriggerVisualizationBuilder(const UHierarchicalTriggerComponent* InComponent)
	: Component(InComponent)
{
	if (InComponent)
	{
		if (const AActor* Owner = InComponent->GetOwner())
		{
			Origin = Owner->GetActorLocation();
		}
	}
}

void FTriggerVisualizationBuilder::BeginModule(const UHierarchicalTriggerComponentModule* InModule, int32 InDepth,
	bool bInHighlighted)
{
	CurrentModule = InModule;
	CurrentDepth = InDepth;
	bCurrentModuleHighlighted = bInHighlighted;
}

void FTriggerVisualizationBuilder::EndModule()
{
	CurrentModule = nullptr;
	CurrentDepth = 0;
	bCurrentModuleHighlighted = false;
}

FTriggerVisualizationHandle FTriggerVisualizationBuilder::AddPrimitive(FTriggerVisualizationPrimitive&& Primitive)
{
	Primitive.Owner = CurrentModule;
	return FTriggerVisualizationHandle(*this, Primitives.Emplace(MoveTemp(Primitive)));
}

FTriggerVisualizationHandle FTriggerVisualizationBuilder::AddLine(const FVector& Start, const FVector& End)
{
	FTriggerVisualizationPrimitive Primitive;
	Primitive.Shape = ETriggerVisualizationShape::Line;
	Primitive.Transform = FTransform(Start);
	Primitive.EndLocation = End;
	return AddPrimitive(MoveTemp(Primitive));
}

FTriggerVisualizationHandle FTriggerVisualizationBuilder::AddArrow(const FVector& Start, const FVector& End)
{
	FTriggerVisualizationPrimitive Primitive;
	Primitive.Shape = ETriggerVisualizationShape::Arrow;
	Primitive.Transform = FTransform(Start);
	Primitive.EndLocation = End;
	return AddPrimitive(MoveTemp(Primitive));
}

FTriggerVisualizationHandle FTriggerVisualizationBuilder::AddSphere(const FVector& Center, double InRadius)
{
	FTriggerVisualizationPrimitive Primitive;
	Primitive.Shape = ETriggerVisualizationShape::Sphere;
	Primitive.Transform = FTransform(Center);
	Primitive.Radius = InRadius;
	return AddPrimitive(MoveTemp(Primitive));
}

FTriggerVisualizationHandle FTriggerVisualizationBuilder::AddBox(const FTransform& InTransform, const FVector& InExtent)
{
	FTriggerVisualizationPrimitive Primitive;
	Primitive.Shape = ETriggerVisualizationShape::Box;
	Primitive.Transform = InTransform;
	Primitive.Extent = InExtent;
	return AddPrimitive(MoveTemp(Primitive));
}

FTriggerVisualizationHandle FTriggerVisualizationBuilder::AddCircle(const FTransform& InTransform, double InRadius)
{
	FTriggerVisualizationPrimitive Primitive;
	Primitive.Shape = ETriggerVisualizationShape::Circle;
	Primitive.Transform = InTransform;
	Primitive.Radius = InRadius;
	return AddPrimitive(MoveTemp(Primitive));
}

FTriggerVisualizationHandle FTriggerVisualizationBuilder::AddPoint(const FVector& Location, double ScreenSize)
{
	FTriggerVisualizationPrimitive Primitive;
	Primitive.Shape = ETriggerVisualizationShape::Point;
	Primitive.Transform = FTransform(Location);
	Primitive.Radius = ScreenSize;
	return AddPrimitive(MoveTemp(Primitive));
}

void FTriggerVisualizationBuilder::AddText(const FVector& Location, const FText& Text, const FLinearColor& Color)
{
	FTriggerVisualizationText& Entry = Texts.AddDefaulted_GetRef();
	Entry.Location = Location;
	Entry.Text = Text;
	Entry.Color = Color;
	Entry.Owner = CurrentModule;
}

#endif // WITH_EDITOR
