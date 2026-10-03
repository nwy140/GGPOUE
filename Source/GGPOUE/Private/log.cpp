/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "log.h"
#include "types.h"
#include "GGPOUE_Settings.h"
#include "Engine/Engine.h"

static char logbuf[4 * 1024 * 1024];

void Log(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   Logv(EGGPOLogVerbosity::VeryVerbose, fmt, args);
   va_end(args);
}
void Log(EGGPOLogVerbosity Verbosity, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   Logv(Verbosity, fmt, args);
   va_end(args);
}

void Logv(EGGPOLogVerbosity Verbosity, const char *fmt, va_list args)
{
#if WITH_EDITOR
   if (GIsEditor)
   {
       // Get the settings object
       // Return if logging is not enabled
      UGGPOUE_Settings* Settings = GetMutableDefault<UGGPOUE_Settings>();
      if (!Settings->LoggingEnabled)
         return;

      // If the message is too verbose for the log settings, return
      if (Verbosity > Settings->LogVerbosity)
         return;

      // Apply the string format
      vsprintf_s(logbuf, ARRAY_SIZE(logbuf), fmt, args);
      FString Message = FString(strlen(logbuf), logbuf);

      Message.InsertAt(0, TEXT("GGPO :: "));

      // GPlayInEditorID was removed from newer Unreal versions. Resolve the
      // current PIE instance through the engine world contexts instead.
      int32 PIEInstance = INDEX_NONE;
      if (GEngine)
      {
          for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
          {
              if (WorldContext.WorldType == EWorldType::PIE)
              {
                  PIEInstance = WorldContext.PIEInstance;
                  break;
              }
          }
      }

      if (PIEInstance >= 0)
      {
          Message.InsertAt(0, FString::Printf(TEXT("PIE %d-"), PIEInstance));
      }

      UE_LOG(LogNet, Display, TEXT("%s"), *Message);
   }
#endif
}

