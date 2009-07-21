/*
	Copyright 2009 Christopher A. Taylor

    This file is part of LibCat.

    LibCat is free software: you can redistribute it and/or modify
    it under the terms of the Lesser GNU General Public License as
	published by the Free Software Foundation, either version 3 of
	the License, or (at your option) any later version.

    LibCat is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public
	License along with LibCat.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "SecureClientDemo.hpp"
#include <iostream>
using namespace std;
using namespace cat;

void SecureClientDemo::OnCookie(u8 *buffer)
{
	cout << "Client: Got cookie from the server" << endl;

	u8 challenge[64 + CAT_S2C_COOKIE_BYTES];

	double t1 = Clock::usec();
	if (!tun_client.GenerateChallenge(challenge, 64))
	{
		cout << "Client: Unable to generate challenge" << endl;
		return;
	}
	memcpy(challenge + 64, buffer, CAT_S2C_COOKIE_BYTES); // copy cookie
	double t2 = Clock::usec();

	cout << "Client: Filling challenge message time = " << (t2 - t1) << " usec" << endl;

	cout << "Client: Sending challenge to server" << endl;

	server_ref->OnPacket(my_addr, challenge, sizeof(challenge));
}

void SecureClientDemo::OnAnswer(u8 *buffer)
{
	double t1 = Clock::usec();
	if (!tun_client.ProcessAnswer(buffer, 96, &auth_enc))
	{
		cout << "Client: Ignoring invalid answer from server" << endl;
		return;
	}
	double t2 = Clock::usec();
	cout << "Client: Processing answer time = " << (t2 - t1) << " usec" << endl;

	OnConnect();
}

void SecureClientDemo::OnConnect()
{
	cout << "Client: Connected!  Sending first ping message" << endl;

	connected = true;

	u8 buffer[1500 + AuthenticatedEncryption::OVERHEAD_BYTES] = {0};

	double t1 = Clock::usec();

	buffer[0] = 0; // type 0 message = proof at offset 5

	*(u32*)&buffer[1] = 1; // counter starts at 1

	// 32 bytes at offset 5 used for proof of key
	if (!auth_enc.GenerateProof(buffer + 5, 32))
	{
		cout << "Client: Unable to generate proof" << endl;
		return;
	}

	// Encrypt it
	auth_enc.Encrypt(buffer, 1500);

	double t2 = Clock::usec();

	cout << "Client: Message 0 construction time = " << (t2 - t1) << " usec" << endl;

	server_ref->OnPacket(my_addr, buffer, sizeof(buffer));
}

void SecureClientDemo::OnSessionMessage(u8 *buffer, int bytes)
{
	cout << "Client: Got pong message from server (" << bytes << " bytes)" << endl;

	if (bytes != 1500)
	{
		cout << "Client: Ignoring truncated session message" << endl;
		return;
	}

	u8 type = buffer[0];

	u32 id = *(u32*)&buffer[1];

	if (id >= 5)
	{
		cout << "Client: Got the last pong from the server!" << endl;
		success = true;
		return;
	}

	++id;

	cout << "Client: Sending ping message #" << id << endl;

	double t1 = Clock::usec();

	u8 response[1500 + AuthenticatedEncryption::OVERHEAD_BYTES] = {0};

	response[0] = 1; // type 1 = no proof in this message, just the counter

	*(u32*)&response[1] = id;

	auth_enc.Encrypt(response, 1500);

	double t2 = Clock::usec();

	cout << "Client: Message " << id << " construction time = " << (t2 - t1) << " usec" << endl;

	server_ref->OnPacket(my_addr, response, sizeof(response));
}

void SecureClientDemo::Reset(SecureServerDemo *cserver_ref, const u8 *server_public_key)
{
	cout << "Client: Reset!" << endl;

	server_ref = cserver_ref;
	server_addr = cserver_ref->GetAddress();
	connected = false;
	my_addr = Address(0x76543210, 0xcdef);
	success = false;

	double t1 = Clock::usec();

	if (!tun_client.Initialize(256, server_public_key, 128))
	{
		cout << "Client: Unable to initialize" << endl;
		return;
	}

	double t2 = Clock::usec();

	cout << "Client: Initialization time = " << (t2 - t1) << " usec" << endl;
}

void SecureClientDemo::SendHello()
{
	cout << "Client: Sending hello message" << endl;

	u8 buffer[CAT_C2S_HELLO_BYTES];

	*(u32*)buffer = getLE(0xca7eed);

	server_ref->OnPacket(my_addr, buffer, sizeof(buffer));
}

void SecureClientDemo::OnPacket(const Address &source, u8 *buffer, int bytes)
{
	cout << "Client: Got packet (" << bytes << " bytes)" << endl;

	if (source != server_addr)
	{
		cout << "Client: Ignoring packet not from server" << endl;
		return;
	}

	if (connected)
	{
		double t1 = Clock::usec();
		if (auth_enc.Decrypt(buffer, bytes))
		{
			double t2 = Clock::usec();
			cout << "Client: Decryption overhead time = " << (t2 - t1) << " usec" << endl;
			OnSessionMessage(buffer, bytes - AuthenticatedEncryption::OVERHEAD_BYTES);
		}
		else
		{
			cout << "Client: Ignoring invalid session message" << endl;
		}
	}
	else
	{
		if (bytes == CAT_S2C_COOKIE_BYTES)
		{
			OnCookie(buffer);
		}
		else if (bytes == 96)
		{
			OnAnswer(buffer);
		}
		else
		{
			cout << "Client: Ignoring unrecognized length packet from server (before connection)" << endl;
		}
	}
}
