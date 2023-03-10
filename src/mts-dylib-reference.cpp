//
// Created by Paul Walker on 12/28/22.
//

#include <iostream>

#define LOGDAT std::cout << __func__ << " " << __FILE__ << ":" << __LINE__ << " | ";
#define LOGFN LOGDAT << std::endl

extern "C"
{
   // Master-side API
   static bool hasMaster{false};
   void MTS_RegisterMaster(void *) {
      LOGFN;
      hasMaster = true;
   }
   void MTS_DeregisterMaster() {
      LOGFN;
      hasMaster = false;
   }
   bool MTS_HasMaster() {
      LOGDAT << hasMaster << std::endl;
      return hasMaster;
   }
   // Don't implement IPC
   bool MTS_HasIPC() {
      LOGFN;
      return false;
   }
   void MTS_Reinitialize() {
      LOGFN;
   }

   int numClients{0};
   int MTS_GetNumClients() {
      return numClients;
   }

   double tuning[128];
   void MTS_SetNoteTunings(const double *d)
   {
      LOGFN;
      for (int i=0; i<128; ++i)
         tuning[i] = d[i];
   }

   void MTS_SetNoteTuning(double f, char idx)
   {
      LOGDAT << f << " " << idx << std::endl;
      tuning[idx] = f;
   }

   char scaleName[256]{0};
   void MTS_SetScaleName(const char *s)
   {
      LOGDAT << s << std::endl;
      strncpy(scaleName, s, 255);
   }

   // Don't implement note filtering or channel specific tuning yet
   void MTS_FilterNote(bool, char , char ){}
   void MTS_ClearNoteFilter() {}
   void MTS_SetMultiChannel(bool, char) {}
   void MTS_SetMultiChannelNoteTunings(const double *, char) {}
   void MTS_SetMultiChannelNoteTuning(double, char, char) {}
   void MTS_FilterNoteMultiChannel(bool, char, char){}
   void MTS_ClearNoteFilterMultiChannel(char) {}

   // Client implementation
   void MTS_RegisterClient() {
      LOGFN;
      numClients++;
   }
   void MTS_DeregisterClient() {
      LOGFN;
      numClients --;
   }
   // Don't implement note filtering
   bool MTS_ShouldFilterNote(char, char) { return false; }
   bool MTS_ShoudlFilterNoteMultiChannel(char, char) { return false; }

   const double *MTS_GetTuningTable() { return &tuning[0]; }
   const double *MTS_GetMultiChannelTuningTable(char) { return &tuning[0]; }
   bool MTS_UseMultiChannelTuning(char) { return false; }
   const char * MTS_GetScaleName() { return scaleName; }

}