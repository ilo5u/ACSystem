// ACSystem.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "system.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	std::wfstream fin;
	fin.open(U("ip.cfg"));
	if (!fin)
		return 1;

	std::wstring address;
	fin >> address;
	fin.close();

	fin.open(U("roomid.cfg"));
	if (!fin)
		return 2;
	int64_t roomid;
	std::vector<int64_t> roomids;
	while (!fin.eof())
	{
		fin >> roomid;
		roomids.push_back(roomid);
	}
	fin.close();

	ACLog aclogger;
	aclogger.Start();

	try
	{
		ACCom accom(address);
		accom.Start().wait();

		ACDbms acdbms;
		bool ret = acdbms.Connect();
		if (!ret)
			return 3;

		ACSystem acsystem{ accom, aclogger, acdbms, roomids };
		acsystem.Wait();

		return 0;
	}
	catch (...)
	{
		return 4;
	}
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
