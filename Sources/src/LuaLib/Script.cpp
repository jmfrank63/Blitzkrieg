#include "Script.h"
#ifdef WIN32
#include <windows.h>
#undef GetObject
#endif
#include <stdio.h>
#include <string.h>

static int Script_LOG(lua_State* state)
{
	Script script(state);
	Script::Object obj = script.GetObject(script.GetTop());
#ifdef WIN32
	OutputDebugStringA(obj.GetString());
	OutputDebugStringA("\n");
#else
	printf("%s\n", obj.GetString());
#endif
	return 0;
}


static void FatalError()
{
	throw -1;
}


void Script::Register( const SRegFunction *pList )
{
	const SRegFunction *pCur = pList;
	for(; pCur->func && pCur->name; ++pCur )
		Register( pCur->name, pCur->func );
}

void Script::Init( bool initStandardLibrary) 
{
	m_ownState = false;

	m_state = lua_open(0);
	m_ownState = true;


	Register("_ERRORMESSAGE", Script_LOG);
}

Script::Script(bool initStandardLibrary)
{
	Init( initStandardLibrary );
}


/**
	@return Retrieves the value at [section].[entry].  If either
		[section] or [entry] doesn't exist, [defaultValue] is returned.
**/
int Script::ConfigGetInteger(const char* section, const char* entry,
							 int defaultValue)
{
	return static_cast<int>(ConfigGetReal(section, entry, defaultValue));
}


/**
	@return Retrieves the value at [section].[entry].  If either
		[section] or [entry] doesn't exist, [defaultValue] is returned.
**/
float Script::ConfigGetReal(const char* section, const char* entry,
							double defaultValue)
{
	AutoBlock block(*this);

	Object obj = GetGlobal(section);
	if (obj.IsNil())
		return (float)defaultValue;
	obj = obj.GetByName(entry);
	if (obj.IsNumber())
		return obj.GetNumber();
	return (float)defaultValue;
}


/**
	@return Retrieves the value at [section].[entry].  If either
		[section] or [entry] doesn't exist, [defaultValue] is returned.
**/
const char* Script::ConfigGetString(const char* section, const char* entry,
									const char* defaultValue)
{
	AutoBlock block(*this);

	Object obj = GetGlobal(section);
	if (obj.IsNil())
		return defaultValue;
	obj = obj.GetByName(entry);
	if (obj.IsString())
		return obj.GetString();
	return defaultValue;
}


/**
	Assigns [value] to [section].[entry].
**/
void Script::ConfigSetInteger(const char* section, const char* entry, int value)
{
	AutoBlock block(*this);

	Object sectionTable = GetGlobal(section);

	if (sectionTable.IsNil())
	{
		sectionTable = GetGlobals().CreateTable(section);
	}

	sectionTable.SetNumber(entry, value);
}


/**
	Assigns [value] to [section].[entry].
**/
void Script::ConfigSetReal(const char* section, const char* entry, double value)
{
	AutoBlock block(*this);

	Object sectionTable = GetGlobal(section);

	if (sectionTable.IsNil())
	{
		sectionTable = GetGlobals().CreateTable(section);
	}

	sectionTable.SetNumber(entry, value);
}


/**
	Assigns [value] to [section].[entry].
**/
void Script::ConfigSetString(const char* section, const char* entry, const char* value)
{
	AutoBlock block(*this);

	Object sectionTable = GetGlobal(section);

	if (sectionTable.IsNil())
	{
		sectionTable = GetGlobals().CreateTable(section);
	}

	sectionTable.SetString(entry, value);
}


/**
	Adds [indentLevel] number of spaces to the file.
**/
static void IndentFile(FILE* file, unsigned int indentLevel)
{
	char spaces[500];
	unsigned int i;
	for (i = 0; i < indentLevel; ++i)
		spaces[i] = ' ';
	spaces[i] = 0;
	fputs(spaces, file);
}


/**
	Writes a Lua object to a text file.
**/
static void WriteObject(Script& script, FILE* file, const char* name,
						Script::Object value, unsigned int indentLevel)
{
	if (value.IsNil())
		return;

	if (value.IsUserData()  ||  value.IsFunction())
	{
		return;
	}

	const unsigned int INDENT_SIZE = 4;
	const unsigned int indentSpaces = indentLevel * INDENT_SIZE;
	IndentFile(file, indentSpaces);
	
	if (name)
		fprintf(file, "%s = ", name);

	if (value.IsNumber())
		fprintf(file, "%.16g", value.GetNumber());

	else if (value.IsString())
		fprintf(file, "\"%s\"", value.GetString());

	else if (value.IsTable())
	{
		fputs("\n", file);
		IndentFile(file, indentSpaces);
		fputs("{\n", file);

		Script::Object table = value;

		int upperIndex = 1;
		bool wroteSemi = false;
		bool hasSequential = false;

		{
			Script::AutoBlock block(script);

			Script::Object value1 = table.GetByIndex(1);
			Script::Object value2 = table.GetByIndex(2);

			if (!value1.IsNil()  &&  !value2.IsNil())
			{
				bool firstSequential = true;
				for (; ; ++upperIndex)
				{
					Script::AutoBlock block(script);

					Script::Object value = table.GetByIndex(upperIndex);

					if (value.IsNil())
						break;

					if (!firstSequential)
						fputs(",\n", file);
					
					WriteObject(script, file, NULL, value, indentLevel + 1);

					firstSequential = false;
				}
			}
		}

		if (upperIndex > 1)
		{
			hasSequential = true;
		}
		
		int i;
		script.PushNil();
		while ((i = script.Next(table.GetStackIndex())) != 0)
		{
			char keyName[255];

			Script::Object key = script.GetObject(script.GetTop() - 1);
			Script::Object value = script.GetObject(script.GetTop());

			if (key.IsNumber())
			{
				if (hasSequential)
				{
					float realNum = key.GetNumber();
					int intNum = (int)realNum;
					if (realNum == (float)intNum)
					{
						if (intNum >= 1  &&  intNum < upperIndex)
						{
							script.Pop();
							continue;
						}
					}
				}

				sprintf_s(keyName, sizeof(keyName), "[%.16g]", key.GetNumber());
			}
			else
			{
				strcpy_s(keyName, sizeof(keyName), key.GetString());
			}

			if (hasSequential  &&  !value.IsNil()  &&  !wroteSemi)
			{
				fputs(", ;\n", file);
				wroteSemi = true;
			}

			WriteObject(script, file, keyName, value, indentLevel + 1);

			fputs(",\n", file);

			script.Pop();
		}

		if (hasSequential  &&  !wroteSemi)
		{
			fputs(",\n", file);
		}
		
		IndentFile(file, indentSpaces);

		if (indentLevel == 0)
		{
			fputs("}\n\n", file);
		}
		else
		{
			fputs("}", file);
		}
	}

	if (indentLevel == 0)
	{
		fputs("\n", file);
	}
}


/**
	Save the complete script state.
**/
void Script::SaveText(const char* filename)
{
	FILE* file = NULL;
	if (fopen_s(&file, filename, "wt") != 0)
		file = NULL;

	AutoBlock block(*this);

	int i;
	Object table = GetGlobals();
	PushNil();
	while ((i = Next(table.GetStackIndex())) != 0)
	{
		Object key = GetObject(GetTop() - 1);
		Object value = GetObject(GetTop());

		if (strcmp(key.GetString(), "_VERSION") != 0)
		{
			WriteObject(*this, file, key.GetString(), value, 0);
		}

		Pop();
	}

	fclose(file);
}
