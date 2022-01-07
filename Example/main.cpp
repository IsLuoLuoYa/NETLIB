#include <stdio.h>
#include "Login.h"
#include "../static_base/BaseControl.h"
#include "../ApplyProto.h"
#include <vector>
#include <string.h>
int main()
{
	FairySunOfNetBaseStart();


	CLogInServer s;
	s.MfStart(0, 4568, "localhost", "userLogin", "111111", "mygame", 0, 4566, 0, 4571);
	pause();
	FairySunOfNetBaseOver();
	return 0;
}