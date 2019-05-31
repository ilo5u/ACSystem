#pragma once

class ACDbms
{
public:
	ACDbms();
	~ACDbms();

	ACDbms(const ACDbms&) = delete;
	ACDbms(ACDbms&&) = delete;
	ACDbms& operator=(const ACDbms&) = delete;
	ACDbms& operator=(ACDbms&&) = delete;

public:
	enum class obj_t
	{
		BILL,
		INVOICE,
		REPORT
	};

public:
	void Insert(obj_t obj, json::value data);

private:
	json::value _data;
};