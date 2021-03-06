/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#include "cmTargetLinkLibrariesCommand.h"

const char* cmTargetLinkLibrariesCommand::LinkLibraryTypeNames[3] =
{
  "general",
  "debug",
  "optimized"
};

// cmTargetLinkLibrariesCommand
bool cmTargetLinkLibrariesCommand
::InitialPass(std::vector<std::string> const& args, cmExecutionStatus &)
{
  // must have one argument
  if(args.size() < 1)
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }

  // but we might not have any libs after variable expansion
  if(args.size() < 2)
    {
    return true;
    }

  // Lookup the target for which libraries are specified.
  this->Target =
    this->Makefile->GetCMakeInstance()
    ->GetGlobalGenerator()->FindTarget(0, args[0].c_str());
  if(!this->Target)
    {
    cmOStringStream e;
    e << "Cannot specify link libraries for target \"" << args[0] << "\" "
      << "which is not built by this project.";
    this->Makefile->IssueMessage(cmake::FATAL_ERROR, e.str());
    cmSystemTools::SetFatalErrorOccured();
    return true;
    }

  // Keep track of link configuration specifiers.
  cmTarget::LinkLibraryType llt = cmTarget::GENERAL;
  bool haveLLT = false;

  // Start with primary linking and switch to link interface
  // specification when the keyword is encountered.
  this->DoingInterface = false;

  // add libraries, nothe that there is an optional prefix 
  // of debug and optimized than can be used
  for(unsigned int i=1; i < args.size(); ++i)
    {
    if(args[i] == "LINK_INTERFACE_LIBRARIES")
      {
      this->DoingInterface = true;
      if(i != 1)
        {
        this->Makefile->IssueMessage(
          cmake::FATAL_ERROR,
          "The LINK_INTERFACE_LIBRARIES option must appear as the second "
          "argument, just after the target name."
          );
        return true;
        }
      }
    else if(args[i] == "debug")
      {
      if(haveLLT)
        {
        this->LinkLibraryTypeSpecifierWarning(llt, cmTarget::DEBUG);
        }
      llt = cmTarget::DEBUG;
      haveLLT = true;
      }
    else if(args[i] == "optimized")
      {
      if(haveLLT)
        {
        this->LinkLibraryTypeSpecifierWarning(llt, cmTarget::OPTIMIZED);
        }
      llt = cmTarget::OPTIMIZED;
      haveLLT = true;
      }
    else if(args[i] == "general")
      {
      if(haveLLT)
        {
        this->LinkLibraryTypeSpecifierWarning(llt, cmTarget::GENERAL);
        }
      llt = cmTarget::GENERAL;
      haveLLT = true;
      }
    else if(haveLLT)
      {
      // The link type was specified by the previous argument.
      haveLLT = false;
      this->HandleLibrary(args[i].c_str(), llt);
      }
    else
      {
      // Lookup old-style cache entry if type is unspecified.  So if you
      // do a target_link_libraries(foo optimized bar) it will stay optimized
      // and not use the lookup.  As there maybe the case where someone has
      // specifed that a library is both debug and optimized.  (this check is
      // only there for backwards compatibility when mixing projects built
      // with old versions of CMake and new)
      llt = cmTarget::GENERAL;
      std::string linkType = args[0];
      linkType += "_LINK_TYPE";
      const char* linkTypeString = 
        this->Makefile->GetDefinition( linkType.c_str() );
      if(linkTypeString)
        {
        if(strcmp(linkTypeString, "debug") == 0)
          {
          llt = cmTarget::DEBUG;
          }
        if(strcmp(linkTypeString, "optimized") == 0)
          {
          llt = cmTarget::OPTIMIZED;
          }
        }
      this->HandleLibrary(args[i].c_str(), llt);
      }
    } 

  // Make sure the last argument was not a library type specifier.
  if(haveLLT)
    {
    cmOStringStream e;
    e << "The \"" << this->LinkLibraryTypeNames[llt]
      << "\" argument must be followed by a library.";
    this->Makefile->IssueMessage(cmake::FATAL_ERROR, e.str());
    cmSystemTools::SetFatalErrorOccured();
    }

  // If the INTERFACE option was given, make sure the
  // LINK_INTERFACE_LIBRARIES property exists.  This allows the
  // command to be used to specify an empty link interface.
  if(this->DoingInterface &&
     !this->Target->GetProperty("LINK_INTERFACE_LIBRARIES"))
    {
    this->Target->SetProperty("LINK_INTERFACE_LIBRARIES", "");
    }

  return true;
}

//----------------------------------------------------------------------------
void
cmTargetLinkLibrariesCommand
::LinkLibraryTypeSpecifierWarning(int left, int right)
{
  cmOStringStream w;
  w << "Link library type specifier \""
    << this->LinkLibraryTypeNames[left] << "\" is followed by specifier \""
    << this->LinkLibraryTypeNames[right] << "\" instead of a library name.  "
    << "The first specifier will be ignored.";
  this->Makefile->IssueMessage(cmake::AUTHOR_WARNING, w.str());
}

//----------------------------------------------------------------------------
void
cmTargetLinkLibrariesCommand::HandleLibrary(const char* lib,
                                            cmTarget::LinkLibraryType llt)
{
  // Handle normal case first.
  if(!this->DoingInterface)
    {
    this->Makefile
      ->AddLinkLibraryForTarget(this->Target->GetName(), lib, llt);
    return;
    }

  // Get the list of configurations considered to be DEBUG.
  std::vector<std::string> const& debugConfigs =
    this->Makefile->GetCMakeInstance()->GetDebugConfigs();
  std::string prop;

  // Include this library in the link interface for the target.
  if(llt == cmTarget::DEBUG || llt == cmTarget::GENERAL)
    {
    // Put in the DEBUG configuration interfaces.
    for(std::vector<std::string>::const_iterator i = debugConfigs.begin();
        i != debugConfigs.end(); ++i)
      {
      prop = "LINK_INTERFACE_LIBRARIES_";
      prop += *i;
      this->Target->AppendProperty(prop.c_str(), lib);
      }
    }
  if(llt == cmTarget::OPTIMIZED || llt == cmTarget::GENERAL)
    {
    // Put in the non-DEBUG configuration interfaces.
    this->Target->AppendProperty("LINK_INTERFACE_LIBRARIES", lib);

    // Make sure the DEBUG configuration interfaces exist so that the
    // general one will not be used as a fall-back.
    for(std::vector<std::string>::const_iterator i = debugConfigs.begin();
        i != debugConfigs.end(); ++i)
      {
      prop = "LINK_INTERFACE_LIBRARIES_";
      prop += *i;
      if(!this->Target->GetProperty(prop.c_str()))
        {
        this->Target->SetProperty(prop.c_str(), "");
        }
      }
    }
}
