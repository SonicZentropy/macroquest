/*
 * MacroQuest2: The extension platform for EverQuest
 * Copyright (C) 2002-2019 MacroQuest Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "pch.h"
#include "MQ2Main.h"

namespace mq {

std::unordered_map<std::string, MQ2Type*> MQ2DataTypeMap;
std::unordered_map<std::string, std::vector<MQ2Type*>> MQ2DataExtensions;

std::recursive_mutex s_variableMutex;

MQ2Type* FindMQ2DataType(const char* Name)
{
	std::scoped_lock lock(s_variableMutex);

	auto iter = MQ2DataTypeMap.find(Name);
	if (iter == MQ2DataTypeMap.end())
		return nullptr;

	return iter->second;
}

bool AddMQ2Type(MQ2Type& Type)
{
	std::scoped_lock lock(s_variableMutex);

	// returns pair with iterator pointing to the constructed
	// element, and a bool indicating if it was actually inserted.
	// this will not replace existing elements.
	auto result = MQ2DataTypeMap.emplace(Type.GetName(), &Type);
	return result.second;
}

bool RemoveMQ2Type(MQ2Type& Type)
{
	std::scoped_lock lock(s_variableMutex);

	// use iterator to erase. allows us to check for existence
	// and erase it without any waste
	const char* thetypename = Type.GetName();
	if (!thetypename)
		return false;

	auto iter = MQ2DataTypeMap.find(thetypename);
	if (iter == MQ2DataTypeMap.end())
		return false;

	// The type existed. Erase it.
	MQ2DataTypeMap.erase(iter);
	return true;
}

MQDataItem* FindMQ2Data(const char* szName)
{
	std::scoped_lock lock(s_variableMutex);

	auto iter = MQ2DataMap.find(szName);
	if (iter == MQ2DataMap.end())
		return nullptr;

	return iter->second.get();
}

bool AddMQ2Data(const char* szName, fMQData Function)
{
	std::scoped_lock lock(s_variableMutex);

	// check if the item exists first, so we don't construct
	// something we don't actually need.
	if (MQ2DataMap.find(szName) != MQ2DataMap.end())
		return false;

	// create new MQ2DATAITEM inside a unique_ptr
	auto newItem = std::make_unique<MQDataItem>();
	strcpy_s(newItem->Name, szName);
	newItem->Function = Function;

	// put the new item into the map
	MQ2DataMap.emplace(szName, std::move(newItem));
	return true;
}

bool RemoveMQ2Data(const char* szName)
{
	std::scoped_lock lock(s_variableMutex);

	auto iter = MQ2DataMap.find(szName);
	if (iter == MQ2DataMap.end())
		return false;

	MQ2DataMap.erase(iter);
	return true;
}

bool AddMQ2TypeExtension(const char* szName, MQ2Type* extension)
{
	// get the extension record for this type name
	auto& record = MQ2DataExtensions[szName];

	// check if we already have this extension added
	if (std::find(record.begin(), record.end(), extension) != record.end())
		return false;

	// insert extension into the record
	record.push_back(extension);
	return true;
}

bool RemoveMQ2TypeExtension(const char* szName, MQ2Type* extension)
{
	// check if we have a record for this type name
	auto iter = MQ2DataExtensions.find(szName);
	if (iter == MQ2DataExtensions.end())
		return false;

	// check if this extension is registered in this record
	auto& record = iter->second;
	auto iter2 = std::find(record.begin(), record.end(), extension);
	if (iter2 == record.end())
		return false;

	// delete the existing record
	record.erase(iter2);

	// if the record is empty, remove it
	if (record.empty())
		MQ2DataExtensions.erase(iter);

	return true;
}

// -1 = no exists, 0 = fail, 1 = success
int FindMacroDataMember(MQ2Type* type, MQTypeVar& Result, char* pStart, char* pIndex,
	bool checkFirst = false)
{
	// search for extensions on this type
	auto extIter = MQ2DataExtensions.find(type->GetName());
	if (extIter != MQ2DataExtensions.end())
	{
		// we have at least one extension. process each one until a match is found
		for (MQ2Type* ext : extIter->second)
		{
			// optimize for failure case, check if exists first
			int result = FindMacroDataMember(ext, Result, pStart, pIndex, true);
			if (result != -1)
				return result;
		}
	}

	// the difference between these two branches is whether we check for existence
	// first or not. Because we assume extensions aren't going to be the common case,
	// we will check for existence first to avoid multiple lookups in failure cases.

	if (checkFirst)
	{
		if (!type->FindMember(pStart) && !type->InheritedMember(pStart))
		{
			return -1;
		}

		return type->GetMember(Result.VarPtr, pStart, pIndex, Result) ? 1 : 0;
	}

	if (type->GetMember(Result.VarPtr, pStart, pIndex, Result))
		return 1;

	if (!type->FindMember(pStart) && !type->InheritedMember(pStart))
	{
		return -1;
	}

	return 0;
}

void DumpWarning(const char* pStart, int index)
{
	if (MQMacroBlockPtr pBlock = GetCurrentMacroBlock())
	{
		MQMacroLine ml = pBlock->Line[index];
		bool oldbAllErrorsDumpStack = std::exchange(bAllErrorsDumpStack, false);
		bool oldbAllErrorsFatal = std::exchange(bAllErrorsFatal, false);

		WriteChatf("\arWARNING: \awUndefined Variable \ag%s\aw used on line %d@%s \ay%s\ax\nMacro Paused.", pStart, ml.LineNumber, ml.SourceFile.c_str(), ml.Command.c_str());

		pBlock->Paused = true;

		bAllErrorsDumpStack = oldbAllErrorsDumpStack;
		bAllErrorsFatal = oldbAllErrorsFatal;
	}
}

static bool FunctionExists(const char* name)
{
	return gMacroBlock
		&& gMacroSubLookupMap.find(name) != gMacroSubLookupMap.end();
}

static bool CallFunction(const char* name, const char* args)
{
	std::vector<std::string> csvColumn;

	const char* myStart = args;
	for (const char* p = myStart; *p; p++)
	{
		if (*p == ',')
		{
			std::string str = std::string(myStart, p - myStart);
			trim(str); // remove leading and trailing spaces

			str.erase(std::remove(str.begin(), str.end(), '\"'), str.end()); // remove double quotes
			csvColumn.push_back(str);
			myStart = p + 1;
		}
	}

	std::string str = myStart;
	if (!str.empty())
	{
		trim(str); // remove leading and trailing spaces

		str.erase(std::remove(str.begin(), str.end(), '\"'), str.end()); // remove double quotes
		csvColumn.push_back(str);
	}

	const auto saved_block = gMacroBlock->CurrIndex;
	const auto pChar = (PSPAWNINFO)pCharSpawn;

	char subLine[MAX_STRING];
	strcpy_s(subLine, name);

	if (!csvColumn.empty())
	{
		strcat_s(subLine, " ");
		for (auto i = csvColumn.begin(); i != csvColumn.end(); i++) {
			strcat_s(subLine, "\"");
			strcat_s(subLine, (*i).c_str());
			strcat_s(subLine, "\"");
			auto j = i;
			j++;
			if (j != csvColumn.end())
				strcat_s(subLine, " ");
		}
	}

	Call(pChar, subLine);

	auto subBlock = gMacroBlock->Line.find(gMacroBlock->CurrIndex);
	subBlock++;

	gMacroBlock->CurrIndex = subBlock->first;
	while (gMacroBlock && subBlock != gMacroBlock->Line.end())
	{
		gMacroStack->LocationIndex = gMacroBlock->CurrIndex;

		// we are in a while loop here, if they do stuff like /mpq or /delay those get thrown out the window.
		DoCommand(pChar, &subBlock->second.Command[0]);

		// it doesnt matter, this is for quick evaluations, if they are using it in any other way its wrong.
		if (!gMacroBlock)
			break;

		if (gMacroBlock->CurrIndex == saved_block)
			return true; // /return happened

		subBlock = gMacroBlock->Line.find(gMacroBlock->CurrIndex);
		subBlock++;

		gMacroBlock->CurrIndex = subBlock->first;
	}

	FatalError("No /return in Subroutine %s", name);
	return false;
}

bool EvaluateDataExpression(MQTypeVar& Result, char* pStart, char* pIndex, bool function_allowed = false)
{
	if (!Result.Type)
	{
		// if they have warnings turned on in the macro, we will disregard checking the map
		if (!gWarning)
		{
			if (gUndeclaredVars.find(pStart) != gUndeclaredVars.end())
				return false; // its a undefined variable no point in moving on further.
		}

		if (MQDataItem* DataItem = FindMQ2Data(pStart))
		{
			if (!DataItem->Function(pIndex, Result))
				return false;
		}
		else if (MQDataVar* DataVar = FindMQ2DataVariable(pStart))
		{
			if (pIndex[0])
			{
				if (DataVar->Var.Type == pArrayType)
				{
					CDataArray* dataArray = static_cast<CDataArray*>(DataVar->Var.Ptr);

					if (!dataArray->GetElement(pIndex, Result))
						return false;
				}
			}
			else
			{
				Result = DataVar->Var;
			}
		}
		else if (function_allowed && FunctionExists(pStart))
		{
			if (!CallFunction(pStart, pIndex))
				return false;

			strcpy_s(DataTypeTemp, gMacroStack->Return.c_str());
			Result.Ptr = &DataTypeTemp[0];
			Result.Type = pStringType;
		}
		else
		{
			if (gMacroBlock)
			{
				if (gWarning)
				{
					DumpWarning(pStart, gMacroBlock->CurrIndex);
				}

				gUndeclaredVars[pStart] = gMacroBlock->CurrIndex;
			}

			return false;
		}
	}
	else
	{
		int result = FindMacroDataMember(Result.Type, Result, pStart, pIndex);
		if (result < 0)
		{
			MQ2DataError("No such '%s' member '%s'", Result.Type->GetName(), pStart);
		}

		if (result <= 0)
			return false;
	}

	return true;
}

bool dataType(const char* szIndex, MQTypeVar& Ret)
{
	if (MQ2Type* pType = FindMQ2DataType(szIndex))
	{
		Ret.Ptr = pType;
		Ret.Type = pTypeType;
		return true;
	}

	return false;
}

void InitializeMQ2Data()
{
	AddMQ2Data("Spawn", dataSpawn);
	AddMQ2Data("Target", dataTarget);
	AddMQ2Data("Me", dataCharacter);
	AddMQ2Data("Spell", dataSpell);
	AddMQ2Data("Switch", dataSwitch);
	AddMQ2Data("Ground", dataGroundItem);
	AddMQ2Data("GroundItemCount", dataGroundItemCount);
	AddMQ2Data("Merchant", dataMerchant);
	AddMQ2Data("PointMerchant", dataPointMerchant);
	AddMQ2Data("Mercenary", dataMercenary);
	AddMQ2Data("Pet", dataPet);
	AddMQ2Data("Window", dataWindow);
	AddMQ2Data("Menu", dataMenu);
	AddMQ2Data("Macro", dataMacro);
	AddMQ2Data("EverQuest", dataEverQuest);
	AddMQ2Data("MacroQuest", dataMacroQuest);
	AddMQ2Data("Math", dataMath);
	AddMQ2Data("Zone", dataZone);
	AddMQ2Data("Group", dataGroup);
	AddMQ2Data("Int", dataInt);
	AddMQ2Data("Bool", dataBool);
	AddMQ2Data("Float", dataFloat);
	AddMQ2Data("Corpse", dataCorpse);
	AddMQ2Data("If", dataIf);
	AddMQ2Data("Cursor", dataCursor);
	AddMQ2Data("NearestSpawn", dataNearestSpawn);
	AddMQ2Data("Type", dataType);
	AddMQ2Data("Time", dataTime);
	AddMQ2Data("GameTime", dataGameTime);
	AddMQ2Data("Ini", dataIni);
	AddMQ2Data("Heading", dataHeading);
	AddMQ2Data("Defined", dataDefined);
	AddMQ2Data("SubDefined", dataSubDefined);
	AddMQ2Data("LastSpawn", dataLastSpawn);
	AddMQ2Data("FindItem", dataFindItem);
	AddMQ2Data("FindItemBank", dataFindItemBank);
	AddMQ2Data("InvSlot", dataInvSlot);
	AddMQ2Data("SelectedItem", dataSelectedItem);
	AddMQ2Data("FindItemCount", dataFindItemCount);
	AddMQ2Data("FindItemBankCount", dataFindItemBankCount);
	AddMQ2Data("Skill", dataSkill);
	AddMQ2Data("AltAbility", dataAltAbility);
	AddMQ2Data("Raid", dataRaid);
	AddMQ2Data("NamingSpawn", dataNamingSpawn);
	AddMQ2Data("SpawnCount", dataSpawnCount);
	AddMQ2Data("LineOfSight", dataLineOfSight);
	AddMQ2Data("Plugin", dataPlugin);
	AddMQ2Data("Select", dataSelect);
	AddMQ2Data("DoorTarget", dataDoorTarget);
	AddMQ2Data("ItemTarget", dataItemTarget);
	AddMQ2Data("DynamicZone", dataDynamicZone);
	AddMQ2Data("Friends", dataFriends);
	AddMQ2Data("Task", dataTask);
	AddMQ2Data("Mount", dataMount);
	AddMQ2Data("Illusion", dataIllusion);
	AddMQ2Data("Familiar", dataFamiliar);
	AddMQ2Data("Alias", dataAlias);
	AddMQ2Data("AdvLoot", dataAdvLoot);
	AddMQ2Data("Alert", dataAlert);
	AddMQ2Data("Range", dataRange);
}

void ShutdownMQ2Data()
{
	std::scoped_lock lock(s_variableMutex);

	MQ2DataMap.clear();
}

bool ParseMQ2DataPortion(char* szOriginal, MQTypeVar& Result)
{
	Result.Type = nullptr;
	Result.Int64 = 0;

	char Index[MAX_STRING] = { 0 };

	// Find [] before a . or null
	char* pPos = &szOriginal[0];
	char* pStart = pPos;
	char* pIndex = &Index[0];
	bool Quote = false;
	bool functionAllowed = false;

	while (true)
	{
		if (*pPos == 0)
		{
			// end completely. process
			if (pStart == pPos)
			{
				if (!Result.Type)
				{
					MQ2DataError("Nothing to parse");
					return false;
				}

				return true;
			}

			if (!EvaluateDataExpression(Result, pStart, pIndex, functionAllowed))
				return false;

			// done processing
			return true;
		}

		if (*pPos == '(')
		{
			*pPos = 0;
			if (pStart == pPos)
			{
				if (!Result.Type)
				{
					MQ2DataError("Encountered typecast without object to cast");
					return false;
				}

				return true;
			}
			else
			{
				if (!EvaluateDataExpression(Result, pStart, pIndex))
					return false;
			}

			if (!Result.Type)
			{
				// error
				return false;
			}

			*pPos = 0;
			++pPos;
			char* pType = pPos;

			while (*pPos != ')')
			{
				if (!*pPos)
				{
					// error
					MQ2DataError("Encountered unmatched parenthesis");
					return false;
				}
				++pPos;
			}

			*pPos = 0;

			MQ2Type* pNewType = FindMQ2DataType(pType);
			if (!pNewType)
			{
				// error
				MQ2DataError("Unknown type '%s'", pType);
				return false;
			}

			if (pNewType == pTypeType)
			{
				Result.Ptr = Result.Type;
				Result.Type = pTypeType;
			}
			else
			{
				Result.Type = pNewType;
			}

			if (pPos[1] == '.')
			{
				++pPos;
				pStart = &pPos[1];
			}
			else if (!pPos[1])
			{
				//Result.Type->ToString(Result.VarPtr,szCurrent);
				return true;
			}
			else
			{
				MQ2DataError("Invalid character found after typecast ')%s'", &pPos[1]);
				return false;
			}
		}
		else
		{
			if (*pPos == '[')
			{
				// index
				*pPos = 0;
				++pPos;
				functionAllowed = true;
				Quote = false;
				bool BeginParam = true;

				while (true)
				{
					if (*pPos == 0)
					{
						MQ2DataError("Unmatched bracket or invalid character following bracket found in index: '%s'", pIndex);
						return false;
					}

					if (BeginParam)
					{
						BeginParam = false;
						if (*pPos == '\"')
						{
							Quote = true;
							++pPos;
							continue;
						}
					}

					if (Quote)
					{
						if (*pPos == '\"')
						{
							if (pPos[1] == ']' || pPos[1] == ',')
							{
								Quote = false;
								++pPos;
								continue;
							}
						}
					}
					else
					{
						if (*pPos == ']')
						{
							if (pPos[1] == '.' || pPos[1] == '(' || pPos[1] == 0)
								break;// valid end
						}
						else if (*pPos == ',')
							BeginParam = true;
					}

					*pIndex = *pPos;
					++pIndex;
					++pPos;
				}

				*pIndex = 0;
				pIndex = &Index[0];
				*pPos = 0;
			}
			else
			{
				if (*pPos == '.')
				{
					// end of this one, but more to come!
					*pPos = 0;
					if (pStart == pPos)
					{
						if (!Result.Type)
						{
							MQ2DataError("Encountered member access without object");
							return false;
						}

						return true;
					}

					if (!EvaluateDataExpression(Result, pStart, pIndex))
						return false;

					pStart = &pPos[1];
					Index[0] = 0;
				}
			}
		}
		++pPos;
	}
}

/**
 * @fn FindMacroClosingBrace
 *
 * @brief Finds the brace that matches from the start position. Returns the match position
 *
 * This is a replication of the original logic for finding the matching brace in MQ2.  It does
 * not allow for escape characters and there is some weird logic to it (for example, quoted
 * strings only last until a close bracket) but I've duplicated it here for backwards
 * compatibility.
 *
 * This will start at the position specified and try to find the matching closing brace.
 * If the match has been found, the function will return the location of that match.  If it
 * cannot be found, returns std::string::npos to match what find() would return.
 *
 * This makes the assumption that you'll be calling it on a position just before ${ since
 * we're trying to match a macro's closing braces and all macro variables start with ${.
 *
 * @param strOrigString The string that you would like to check
 * @param iCurrentPosition The position to start at in the string (allows for starting
 *        in the middle to get an interior match)
 *
 * @return size_t position of the matching brace (or npos if no match found)
 */
size_t FindMacroClosingBrace(std::string_view strOrigString, size_t iCurrentPosition)
{
	// Setup our needed trackers
	int iBraceTracker = 0; // Track {
	bool bQuoteTracker = false; // Track "
	bool bParamTracker = false; // Track parameters ([ & ,)

	// We already know what the first two characters are going to be so let's jump to the new position + 2.
	iCurrentPosition += 2;

	// Since we skipped the brace, let's increment that.
	iBraceTracker = 1;

	// Walk through the braces until we find the matching end brace or we reach the end of the string
	while ((iBraceTracker > 0) && (iCurrentPosition < strOrigString.size()))
	{
		// If our last character was a start parameter
		if (bParamTracker)
		{
			// This character isn't a start parameter
			bParamTracker = false;
			// If this is a double quote after the parameter, we're going to assume we're in a quoted string
			if (strOrigString[iCurrentPosition] == '"')
			{
				bQuoteTracker = true;
			}
		}
		// If our last character wasn't a start parameter, are we in a quoted string?
		else if (bQuoteTracker)
		{
			// If this character is a quote
			if (strOrigString[iCurrentPosition] == '"')
			{
				// If there is room for another character at the end fo this string and that character is ] or ,
				if (((iCurrentPosition + 1) < strOrigString.size())
					&& (strOrigString[iCurrentPosition + 1] == ']' || strOrigString[iCurrentPosition + 1] == ','))
				{
					// Assume we're no longer in a quoted string.
					bQuoteTracker = false;
				}
			}
		}
		// Otherwise our last character wasn't a parameter, and we're not in a quoted string
		else
		{
			// Decrement for a close brace
			if (strOrigString[iCurrentPosition] == '}')
			{
				iBraceTracker--;
			}
			// Increment for an open brace
			else if (strOrigString[iCurrentPosition] == '{')
			{
				iBraceTracker++;
			}
			// Check for open parameters
			else if (strOrigString[iCurrentPosition] == '[' || strOrigString[iCurrentPosition] == ',')
			{
				bParamTracker = true;
			}
		}

		// Move to the next character
		iCurrentPosition++;
	}

	// If we found our end brace, return the position in the string.
	if (iBraceTracker == 0)
	{
		return iCurrentPosition;
	}

	// If we didn't find our end brace, return npos
	return std::string::npos;
}

/**
 * @fn GetMacroVarData
 *
 * @brief Wrapper for ParseMQ2DataPortion starting with a var string
 *
 * ParseMQ2DataPortion is used in other parts of the code and handles parsing using
 * a character array.  This function starts as a string since we're using
 * a string builder in other areas and just wraps that check so that we don't
 * have to modify ParseMQ2DataPortion.
 *
 * It also validates that it is actually a variable and strips ${ and } to prepare
 * it for ParseMQ2DataPortion.
 *
 * @param strVarToParse The variable to parse (including ${ })
 *
 * @return std::string The parsed variable (or NULL if it was invalid).
 */
std::string GetMacroVarData(std::string_view strVarToParse)
{
	// Start by setting our return to NULL (in case of invalid variables)
	std::string strReturn = "NULL";

	// Check to make sure this has the starting marks of a variable (if we got here it should, but just in case)
	if (strVarToParse.substr(0, 2) == "${"
		&& strVarToParse.substr(strVarToParse.length() - 1) == "}")
	{
		// Strip the ${ and } off of the variable to pass it to ParseMQ2DataPortion
		strVarToParse = strVarToParse.substr(2, strVarToParse.length() - 3);

		// TODO:  Check to see if this exceeds MAX_STRING and error.
		// Create a place to hold our "current" string and make sure its long enough to pass on (these
		// functions expect a string buffer of MAX_STRING length).
		std::string currentStr{ strVarToParse };
		currentStr.resize(MAX_STRING);

		MQTypeVar Result;

		// If the parse was successful and there is a result type and we could convert that type to a string
		if (ParseMQ2DataPortion(&currentStr[0], Result) && Result.Type && Result.Type->ToString(Result.VarPtr, &currentStr[0]))
		{
			// Set our return whatever szCurrent was modified to be
			strReturn = currentStr;
		}
	}
	return strReturn;
}

/**
 * @fn GetParseDelimiterPos
 *
 * @brief Get the position of the Parse Delimiter
 *
 * This is with regard to the implementation of the ${Parse[x,ThingToParse]}
 * functionality.  Since the parse delimiter can be either a comma or a space
 * this function will figure out which of the two comes first.
 *
 * Note that there is no error handling on this to make sure you passed in
 * a ${Parse so passing anythin gelse into this is going to to just tell you
 * where the first comma or space in your string is.
 *
 * @param strOriginal The string you want to check for the first comma or space
 * @param iStartPos Where in the string you want to start
 *
 * @return size_t The location found or std::string::npos
 */
size_t GetParseDelimiterPos(std::string_view strOriginal, size_t iStartPos = 0)
{
	// Setup the location of the first delimiter (and default it to comma)
	size_t iFirstDelimiter = strOriginal.find(',', iStartPos);

	// Find the position of the first space
	const size_t iFirstSpace = strOriginal.find(' ', iStartPos);

	// If we found both a comma and a space
	if (iFirstDelimiter != std::string::npos && iFirstSpace != std::string::npos)
	{
		// If the comma is further than the space...
		if (iFirstDelimiter > iFirstSpace)
		{
			// Set the first delimiter to the position of the space
			iFirstDelimiter = iFirstSpace;
		}
	}
	// If we only found a space
	else if (iFirstSpace != std::string::npos)
	{
		// Set the First Delimiter to the position of the space
		iFirstDelimiter = iFirstSpace;
	}

	// Return whatever we found (note if we found nothing this will be std::string::npos)
	return iFirstDelimiter;
}

/**
 * @fn HandleParseParam
 *
 * @brief Parses the ${Parse[ parameter
 *
 * This is with regard to the implementation of the ${Parse[x,ThingToParse]}
 * functionality which allows you to choose how many iterations you would like
 * to parse a variable.
 *
 * It's called HandleParseParam instead of ParseParseParam for obvious reasons.
 *
 * It will take a string that contains a ${Parse parameter and return the parsed
 * version of that string based on the number of iterations specified.  If this
 * is the outer loop, it should be all iterations, and if it is the inner loop
 * it should only be one iteration.
 *
 * It will stop parsing after the minimum number of parses to meet the requirement
 * so if you pass it ${Parse[300,MyThing]} It's only going to parse once since
 * it doesn't need to parse any more than that.
 *
 * It is built to handle nested parse parameters by passing those back to
 * ModifyMacroString.
 *
 * The function accepts an optional escape string (default \) to allow you to force
 * accept a character.
 *
 * Note that the defaults are defined in the header
 *
 * @param strOriginal The string to parse
 * @param bParseOnce Whether to parse just once or parse all iterations (default false - all iterations)
 *
 * @return std::string The parsed string (or the original string if there was no ${Parse parameter)
 */
std::string HandleParseParam(std::string_view strOriginal, const bool bParseOnce)
{
	// Setup a return string to handle our return and initialize it to the original string.
	std::string strReturn{ strOriginal };

	// If the string passed to us doesn't start with ${Parse[
	if (strReturn.substr(0, 8) != PARSE_PARAM_BEG)
	{
		// Check if there is a Parse deeper in (otherwise there's nothing to do).
		if (strReturn.find(PARSE_PARAM_BEG) != std::string::npos)
		{
			// If we're not at the start of a variable...
			if (strReturn.substr(0, 2) != "${")
			{
				// Tokenize the entire thing and parse it that way
				strReturn = ModifyMacroString(strReturn, bParseOnce);
			}
			// We are at the start of a variable
			else
			{
				// We're going to need to start further in so that we can tokenize the internals
				// This takes care of situations like ${SomeCustomTLO[${Parse[0,${Me.Name}]}, ${Me.Name}]}
				strReturn = "${" + ModifyMacroString(strReturn.substr(2, strReturn.length()), bParseOnce);

				// If we are supposed to parse until we're done we need to do a final evaluation of the variable we found.
				if (!bParseOnce)
				{
					// Evaluate the (parsed) return string.
					strReturn = GetMacroVarData(strReturn);
				}
			}
		}
	}
	// The string starts with ${Parse[
	else
	{
		// Get the position of the first bracket (note: we could assume this, but we're going to change it later anyway so I think it's okay)
		size_t iFirstBracket = strReturn.find('[');

		// If we didn't find the first bracket, we can't move on with the parsing, so check that we did find it
		if (iFirstBracket != std::string::npos)
		{
			// Now we need to know where the first delimiter is
			size_t iFirstDelimiter = GetParseDelimiterPos(strReturn);

			// Again, we can't move on unless we found it -- this stops syntax errors like ${Parse[test]}
			if (iFirstDelimiter != std::string::npos)
			{
				// Save what the delimiter actually is because we're going to need it later
				std::string strDelimiter = strReturn.substr(iFirstDelimiter, 1);

				// Get between the bracket and the first delimiter and save that int as our number of iterations
				int iParseIterations = GetIntFromString(strReturn.substr(iFirstBracket + 1, iFirstDelimiter - 1 - iFirstBracket), 0);

				do {
					// Find the first bracket in the return string
					iFirstBracket = strReturn.find('[');

					// Find the first delimiter in the return string
					iFirstDelimiter = GetParseDelimiterPos(strReturn);

					// Set the delimiter
					strDelimiter = strReturn.substr(iFirstDelimiter, 1);

					// We can assume the above three things exist because we're the ones creating them from here on out
					// and we checked them before we got to this loop.

					// The Sub Parse (thing to be parsed) is the area after the first Delimiter.
					std::string strSubParse = strReturn.substr(iFirstDelimiter + 1, strReturn.length() - iFirstDelimiter - 3);

					// If this is a ${Parse[0, just remove the parse because we're done.  Also, if this is a negative
					// number, treat it like a Parse 0.
					if (iParseIterations <= 0)
					{
						// Remove the parse
						strReturn = strSubParse;
					}
					else
					{
						// This is more than a Parse 0.
						// If there's not a variable to parse...
						if (strSubParse.find("${") == std::string::npos)
						{
							// If we're parsing forever, then let's reduce the count to one since there's nothing to
							// parse, skipping all the way to the ${Parse[0, instead of running through all of the
							// iterations we have left when nothing will change
							if (!bParseOnce)
							{
								iParseIterations = 1;
							}

							strReturn = PARSE_PARAM_BEG + "0" + strDelimiter + strSubParse + PARSE_PARAM_END;
						}
						else
						{
							// We have variables to parse. Decrement the iterations and Parse the variables only once
							// (we'll go further if we need to in additional loops)
							strReturn = PARSE_PARAM_BEG + std::to_string(iParseIterations - 1) + strDelimiter
								+ ModifyMacroString(strSubParse, true) + PARSE_PARAM_END;
						}
					}

					// Decrement our iteration counter
					iParseIterations--;

				} while (iParseIterations >= 0 && !bParseOnce); // Stop when we've passed 0 or if we're only supposed to parse once
			}
		}
	}

	return strReturn;
}

/**
 * @fn ParseMacroVar
 *
 * @brief Parses a Macro Variable without tokenizing first, supports recursion
 *
 * ParseMacroVar parses a full variable.  In the case of a nested full variable like:
 *         ${SomeOperation[${Me.Name}]}
 * the function will parse the rightmost variable first and work its way back to the
 * left side.  This should result in getting the innermost variables first.  The
 * function is setup to recurse unless a ${Parse[ parameter is passed to it or if
 * bParseOnce is set to true, in which case it only parses the first pass.  This means
 * the nested variables would get parsed and evaluated, but the unnested variables would
 * not get evaluated.
 *
 * In the case of a ${Parse[ parameter, the function will pass handling off to
 * HandleParseParam if it is found immediately, or during recursion if it is found buried.
 *
 * Where ModifyMacroString will tokenize the string and find the longest variables
 * before passing them in whole to ParseMacroVar, ParseMacroVar expects that it
 * is being passed an already tokenized variable.  While ParseMacroVar could be
 * part of ModifyMacroString, it's easier for troubleshooting to keep it as a
 * separate operation (and also allows us to skip the first iteration of
 * tokenization if we know we have a full variable already).
 *
 * @param strOriginal The string to parse
 * @param bParseOnce Whether to parse just once or parse all iterations (default false - all iterations)
 *
 * @return std::string The parsed string
 */
std::string ParseMacroVar(std::string_view strOriginal, const bool bParseOnce = false)
{
	// Setup a return string and initialize it to the original string
	std::string strReturn{ strOriginal } ;

	// If there is no parse parameter
	if (strOriginal.find(PARSE_PARAM_BEG) == std::string::npos)
	{
		// Track our position and we're starting from the right
		size_t iCurrentPosition = strReturn.length();

		// Loop until we reach the beginning of the string
		while (iCurrentPosition > 0)
		{
			// Starting from one position left of our current position in the string, find the farthest right ${.
			const size_t iPosition = strReturn.rfind("${", iCurrentPosition - 1);

			// If we found a variable marker
			if (iPosition != std::string::npos)
			{
				// Find the closing brace
				const size_t iCloseBrace = FindMacroClosingBrace(strReturn, iPosition);

				// If we found the Closing Brace then we can get the variable's data
				if (iCloseBrace != std::string::npos)
				{
					// If the Closing Brace is AFTER our last parsed variable and we're only
					// parsing once then we need to skip parsing this. This accounts for situations
					// like ${Parse[1,${Spawn[=${Me.Name}].ID]}
					if (!(bParseOnce && (iCloseBrace > iCurrentPosition)))
					{
						// We're going to use this value a couple times so store it in a variable
						std::string strVarToParse = strReturn.substr(iPosition, iCloseBrace - iPosition);

						// Parse the variable (also going to use this a couple of times)
						std::string strParsedVar = GetMacroVarData(strVarToParse);

						// If the variable changed (otherwise no point in doing anything)
						if (strVarToParse != strParsedVar)
						{
							// If the variable contains a ${ and we are not in a parse once then we need to
							// send it through the parser again
							if (!bParseOnce && (strParsedVar.find("${") != std::string::npos))
							{
								strParsedVar = ModifyMacroString(strParsedVar);
							}

							// Replace the variable in our return string with the parsed variable
							strReturn.replace(iPosition, iCloseBrace - iPosition, strParsedVar);
						}
					}
				}

				// In any case, move our cursor past the current position.
				iCurrentPosition = iPosition;
			}
			else
			{
				// Otherwise we didn't find a variable marker so we're done.
				iCurrentPosition = 0;
			}
		}
	}
	else
	{
		// There is a parse parameter in this string
		strReturn = HandleParseParam(strOriginal, bParseOnce);
	}

	return strReturn;
}

/**
 * @fn ModifyMacroString
 *
 * @brief Tokenizes a mixed string and passes whole variables for operations
 *
 * The defaults for this function are set in the header.
 *
 * This function takes a string and tokenizes it to the longest whole variable
 * it can find, then passes each of those whole variables to an operation as
 * specified.  The default operation is to Parse.
 *
 * The results are concatenated until the entire string has been parsed and the
 * return value is the entire string after the modification.
 *
 * If there are mismatched braces and it cannot find a whole variable, it will
 * return the remainder of the string.  This behavior will cause this function
 * to parse as many (whole) variables as it can find, even if there are errors
 * in other variables, but a mismatch will cause the return of the remaining
 * (unparsed) string.
 *
 * @param strOriginal The string to parse
 * @param bParseOnce Whether to parse just once or parse all iterations (default false - all iterations)
 * @param iOperation What operation to perform, available operations are:
 *         -2 - Wrap Parse Zero, No Doubles - Wrap variables in a Parse Zero (unless they already have a Parse zero)
 *         -1 - Default - Parse variables using ParseMacroVar
 *          0 - Wrap Parse Zero - Wrap variables in a Parse Zero (don't parse)
 *
 * @return std::string The parsed string
 */
std::string ModifyMacroString(std::string_view strOriginal, bool bParseOnce, ModifyMacroMode iOperation)
{
	// Setup a return variable to track our string being built
	std::string strReturn;

	// Start at the beginning
	size_t iCurrentPosition = 0;

	// While we have ${ sections
	while (iCurrentPosition != std::string::npos)
	{
		// Find the next ${
		const size_t iNewPosition = strOriginal.find("${", iCurrentPosition);

		// If we couldn't find ${ by the end of the string
		if (iNewPosition == std::string::npos)
		{
			// Add the rest of the line
			strReturn += strOriginal.substr(iCurrentPosition);
			iCurrentPosition = std::string::npos;
		}
		else
		{
			// We found a ${ - If the new position skips from our old position
			if (iNewPosition > iCurrentPosition)
			{
				// Catch the data we missed from the Current Position to the New Position
				strReturn += strOriginal.substr(iCurrentPosition, (iNewPosition - iCurrentPosition));
			}

			// Advance the current pointer to where we are now.
			iCurrentPosition = iNewPosition;

			// Get the matching brace position.
			const size_t iBracePosition = FindMacroClosingBrace(strOriginal, iCurrentPosition);

			// If we didn't find the matching brace, return the rest of the string
			if (iBracePosition == std::string::npos)
			{
				strReturn += strOriginal.substr(iCurrentPosition);

				// We reached the end of the string
				iCurrentPosition = std::string::npos;
			}
			else
			{
				// We found the matching brace
				switch (iOperation)
				{
					// Wrap Parse Zero, No Doubles - Wrap variables in a Parse Zero (unless they already have a Parse zero)
				case ModifyMacroMode::WrapNoDoubles:
					// If we already have a Parse Zero
					if (strOriginal.substr(iCurrentPosition, PARSE_PARAM_BEG.length() + 1) == PARSE_PARAM_BEG + "0")
					{
						// Just add the section as is
						strReturn += strOriginal.substr(iCurrentPosition, (iBracePosition - iCurrentPosition));
					}
					else
					{
						// Add a Parse Zero
						strReturn.append(PARSE_PARAM_BEG);
						strReturn.append("0,");
						strReturn.append(strOriginal.substr(iCurrentPosition, (iBracePosition - iCurrentPosition)));
						strReturn.append(PARSE_PARAM_END);
					}
					break;

					// 0 - Wrap Parse Zero - Wrap variables in a Parse Zero (don't parse)
				case ModifyMacroMode::Wrap:
					strReturn.append(PARSE_PARAM_BEG);
					strReturn.append("0,");
					strReturn.append(strOriginal.substr(iCurrentPosition, (iBracePosition - iCurrentPosition)));
					strReturn.append(PARSE_PARAM_END);
					break;

					// Default case is Parse
				case ModifyMacroMode::Default:
				default:
					// Parse it and add the result to our current string
					strReturn += ParseMacroVar(strOriginal.substr(iCurrentPosition, (iBracePosition - iCurrentPosition)), bParseOnce);
				}

				// Advance our position to where the brace is
				iCurrentPosition = iBracePosition;
			}
		}
	}

	// Return the parsed string
	return strReturn;
}

/**
 * @fn ParseMacroData
 *
 * @brief Backwards compatible wrapper for ModifyMacroString
 *
 * This function is a backwards compatible wrapper for ModifyMacroString
 * that takes the place of the original ParseMacroData function so that
 * code being passed to the parser doesn't have to be rewritten.
 *
 * It will perform the same function as the original (parsing the char*
 * and storing it in the original location) but it does so by converting
 * to a string and passing it to ModifyMacroString instead of doing the
 * parsing itself.
 *
 * The original ParseMacroData function would return false if there are
 * no braces left to Parse, but this function only returns "true" as an
 * indicator of success rather than an indicator of "continue to parse"
 * since that is all handled in the macro language itself now.
 *
 * @param szOriginal The char* to parse and store the output
 * @param BufferSize Not used, maintained for backwards compatibility
 *
 * @return bool (MQ2) Success
 */
bool ParseMacroData(char* szOriginal, size_t BufferSize)
{
	if (gParserVersion == 2)
	{
		// Pass it off to our String Parser
		std::string strReturn = ModifyMacroString(szOriginal);

		// If the result is larger than MAX_STRING
		if (strReturn.length() >= MAX_STRING)
		{
			// If we are currently in a macro block
			if (MQMacroBlockPtr currblock = GetCurrentMacroBlock())
			{
				MacroError("Data Truncated in %s, Line: %d.  Expanded Length was greater than %d",
					currblock->Line[currblock->CurrIndex].SourceFile.c_str(),
					currblock->Line[currblock->CurrIndex].LineNumber, MAX_STRING);
			}

			// Trim the result.
			strReturn = strReturn.substr(0, MAX_STRING - 1);
		}

		// Copy the parsed string into the original string
		strcpy_s(szOriginal, strReturn.length() + 1, strReturn.c_str());
		return true;
	}

	// find each {}
	char* pBrace = strstr(szOriginal, "${");
	if (!pBrace)
		return false;

	bool Changed = false;
	char szCurrent[MAX_STRING] = { 0 };

	do
	{
		// find this brace's end
		char* pEnd = &pBrace[1];
		bool Quote = false;
		bool BeginParam = false;
		int nBrace = 1;

		while (nBrace)
		{
			++pEnd;

			if (BeginParam)
			{
				BeginParam = false;

				if (*pEnd == '\"')
				{
					Quote = true;
				}
				continue;
			}

			if (*pEnd == 0)
			{
				// unmatched brace or quote
				goto pmdbottom;
			}

			if (Quote)
			{
				if (*pEnd == '\"')
				{
					if (pEnd[1] == ']' || pEnd[1] == ',')
					{
						Quote = false;
					}
				}
			}
			else
			{
				if (*pEnd == '}')
				{
					nBrace--;
				}
				else if (*pEnd == '{')
				{
					nBrace++;
				}
				else if (*pEnd == '[' || *pEnd == ',')
					BeginParam = true;
			}

		}

		*pEnd = 0;

		strcpy_s(szCurrent, &pBrace[2]);
		if (szCurrent[0] == 0)
		{
			goto pmdbottom;
		}

		if (ParseMacroData(szCurrent, sizeof(szCurrent)))
		{
			size_t NewLength = strlen(szCurrent);
			memmove(&pBrace[NewLength + 1], &pEnd[1], strlen(&pEnd[1]) + 1);
			int addrlen = (int)(pBrace - szOriginal);
			memcpy_s(pBrace, BufferSize - addrlen, szCurrent, NewLength);
			pEnd = &pBrace[NewLength];
			*pEnd = 0;
		}

		{
			MQTypeVar Result;

			if (!ParseMQ2DataPortion(szCurrent, Result) || !Result.Type || !Result.Type->ToString(Result.VarPtr, szCurrent))
			{
				strcpy_s(szCurrent, "NULL");
			}
		}

		size_t NewLength = strlen(szCurrent);
		int endlen = strlen(&pEnd[1]) + 1;

		memmove(&pBrace[NewLength], &pEnd[1], endlen);

		int addrlen = (int)(pBrace - szOriginal);
		if (NewLength > BufferSize - addrlen)
		{
			if (MQMacroBlockPtr currblock = GetCurrentMacroBlock())
			{
				SyntaxError("Syntax Error: %s Line:%d in %s\nNewLength %d was greater than BufferSize - addrlen %d in ParseMacroData, did you try to read data that exceeds 2048 from your macro?",
					currblock->Line[currblock->CurrIndex].Command.c_str(),
					currblock->Line[currblock->CurrIndex].LineNumber,
					currblock->Line[currblock->CurrIndex].SourceFile.c_str(),
					NewLength, BufferSize - addrlen);
			}

			NewLength = BufferSize - addrlen;
		}

		memcpy_s(pBrace, BufferSize - addrlen, szCurrent, NewLength);

		if (!bAllowCommandParse)
		{
			bAllowCommandParse = true;
			Changed = false;
			break;
		}
		else
		{
			Changed = true;
		}

	pmdbottom:
		;
	} while (pBrace = strstr(&pBrace[1], "${"));

	if (Changed)
	{
		while (ParseMacroData(szOriginal, BufferSize)) {}
	}

	return Changed;
}

} // namespace mq
