// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Flint.h"
#include "FlintGameMode.h"
#include "FlintCharacter.h"

AFlintGameMode::AFlintGameMode()
{
	// set default pawn class to our character
	DefaultPawnClass = AFlintCharacter::StaticClass();	
}
