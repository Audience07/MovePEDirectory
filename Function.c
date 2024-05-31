#include "head.h"



//���ļ������仺�����������ļ�������ָ��
//��׼�������½ڣ��������һ�����������½ڵĴ�С
LPVOID _OpenFile(IN const LPSTR str, IN size_t SizeOfNewSection) {
	size_t FileSize;
	FILE* pf = fopen(str, "rb");
	if (!pf) {
		perror("���ļ�ʧ��");
		return NULL;
	}
	fseek(pf, 0, SEEK_END);
	FileSize = ftell(pf);
	fseek(pf, 0, SEEK_SET);

	LPVOID FileBuffer = malloc((FileSize+SizeOfNewSection));
	if (!FileBuffer) {
		printf("����ռ�ʧ��\n");
		fclose(pf);
		free(FileBuffer);
		return 0;
	}

	fread(FileBuffer, 1, FileSize, pf);
	if (!FileBuffer) {
		printf("��ȡ�ڴ�ʧ��\n");
		fclose(pf);
		free(FileBuffer);
		return 0;
	}
	fclose(pf);
	return FileBuffer;

}

LPVOID _vOpenFile(IN const LPSTR str, IN size_t SizeOfNewSection) {
	size_t FileSize;
	FILE* pf = fopen(str, "rb");
	if (!pf) {
		perror("���ļ�ʧ��");
		return NULL;
	}
	fseek(pf, 0, SEEK_END);
	FileSize = ftell(pf);
	fseek(pf, 0, SEEK_SET);

	LPVOID FileBuffer = VirtualAlloc(NULL, FileSize + SizeOfNewSection, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	//((FileSize + SizeOfNewSection));
	if (!FileBuffer) {
		printf("����ռ�ʧ��\n");
		fclose(pf);
		free(FileBuffer);
		return 0;
	}

	fread(FileBuffer, 1, FileSize, pf);
	if (!FileBuffer) {
		printf("��ȡ�ڴ�ʧ��\n");
		fclose(pf);
		free(FileBuffer);
		return 0;
	}
	fclose(pf);
	return FileBuffer;

}





//��ȡ�ļ���ʶ���洢��FileSign�ṹ�У����ؽڱ�����
size_t _ReadData(LPVOID FileBuffer, struct FileSign* FileSign) {
	FileSign->MZHeader = *(WORD*)((char*)FileBuffer);
	if (FileSign->MZHeader != 0x5a4d) {
		return 0;
	}
	//��λָ��
	FileSign->NTHeader = (char*)((char*)FileBuffer + (*(DWORD*)((char*)FileBuffer + 0x3C)));
	FileSign->PEHeader = (char*)((char*)FileSign->NTHeader + 0x4);
	FileSign->OptionalHeader = (char*)((char*)FileSign->NTHeader + 0x18);

	//PEͷ
	FileSign->Machine = *(WORD*)((char*)FileSign->PEHeader);
	FileSign->NumberOfSection = *(WORD*)((char*)FileSign->PEHeader + 0x2);
	FileSign->SizeOfOptionHeader = *(WORD*)((char*)FileSign->PEHeader + 0x10);

	//��ѡPEͷ
	FileSign->Magic = *(WORD*)((char*)FileSign->OptionalHeader);
	FileSign->EntryPoint = *(DWORD*)((char*)FileSign->OptionalHeader + 0x10);
	FileSign->ImageBase = *(DWORD*)((char*)FileSign->OptionalHeader + 0x1C);
	FileSign->SectionAlignment = *(DWORD*)((char*)FileSign->OptionalHeader + 0x20);
	FileSign->FileAlignment = *(DWORD*)((char*)FileSign->OptionalHeader + 0x24);
	FileSign->SizeOfImage = *(DWORD*)((char*)FileSign->OptionalHeader + 0x38);
	FileSign->SizeOfHeaders = *(DWORD*)((char*)FileSign->OptionalHeader + 0x3C);

	//���ؽڱ�����
	return 1;
}






//��ȡ�ڱ��ؼ��ֶ�
void _ReadSectionTable(OUT struct SectionTable* pSectionTable,IN struct FileSign* pFileSign) {
	for (int i = 0; i < pFileSign->NumberOfSection;i++, pSectionTable++) {
		pSectionTable->Point = (char*)((char*)pFileSign->OptionalHeader + pFileSign->SizeOfOptionHeader + (i * 0x28));
		memcpy(pSectionTable->name, pSectionTable->Point, 8);
		pSectionTable->VirtualSize = *(DWORD*)((char*)pSectionTable->Point + 0x8);
		pSectionTable->VirtualAddress = *(DWORD*)((char*)pSectionTable->Point + 0xC);
		pSectionTable->SizeOfRawData = *(DWORD*)((char*)pSectionTable->Point + 0x10);
		pSectionTable->PointToRawData = *(DWORD*)((char*)pSectionTable->Point + 0x14);
		pSectionTable->Characteristics = *(DWORD*)((char*)pSectionTable->Point + 0x24);
	}
}


//�����������ļ���ȡ������Ŀ�ִ�пɶ�д�ڴ���
LPVOID _vFileBuffer(IN LPVOID FileBuffer, IN struct FileSign* pFileSign, IN struct SectionTable* pSectionTable) {
	LPVOID vFileBuffer = VirtualAlloc(NULL, pFileSign->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!vFileBuffer) {
		return NULL;
	}
	memset(vFileBuffer, 0, pFileSign->SizeOfImage);
	memcpy(vFileBuffer, FileBuffer, pFileSign->SizeOfHeaders);
	for (int i = 0; i < pFileSign->NumberOfSection; i++) {
		memcpy(((char*)vFileBuffer + pSectionTable->VirtualAddress), ((char*)FileBuffer + pSectionTable->PointToRawData), pSectionTable->SizeOfRawData);
		pSectionTable++;
	}
	return vFileBuffer;

}


//��ת��EntryPoint����
//void _Run(IN struct FileSign* pFileSign, IN LPVOID vFileBuffer) {
//	DWORD EntryPoint = (char*)vFileBuffer + pFileSign->EntryPoint;
//	_asm{
//		mov eax, EntryPoint;
//		jmp eax;
//	}
//
//}



//���ش������
size_t _FindCodeSection(IN struct FileSign* pFileSign,IN struct SectionTable* pSectionTable) {
	for (int i = 0; i < pFileSign->NumberOfSection; i++) {
		if ((pFileSign->EntryPoint > pSectionTable->VirtualAddress) && (pFileSign->EntryPoint < (pSectionTable->VirtualAddress + pSectionTable->SizeOfRawData))) {
			return i;
		}
		pSectionTable++;
	}
}


//���ɸ���,��shellcodeд�����ν�β
void _WriteShellCodeToIdleArea(OUT LPVOID vFileBuffer,IN struct FileSign* pFileSign,IN struct SectionTable* pSectionTable,IN char* shellcode , IN size_t SizeOfCode) {
	//�жϴ����
	size_t n = _FindCodeSection(pFileSign, pSectionTable);
	pSectionTable += n;

	
	//�ж�ʣ��ռ��Ƿ�����shellcode
	LPVOID BeginCode = ((char*)vFileBuffer + pSectionTable->VirtualAddress + pSectionTable->VirtualSize);
	if (SizeOfCode >= ((DWORD)vFileBuffer + (pSectionTable->VirtualAddress + pSectionTable->SizeOfRawData) - (DWORD)BeginCode)) {
		printf("�����ʣ��ռ䲻��\n");
		return;
	}

	//��дshellcode
	memcpy(BeginCode, shellcode, SizeOfCode);


	//��ת����ImageBuffer����
	//����call		FunctionAddress-(BeginCode+Push+5-ImageBuffer+ImageBase)	MessageBoxA���ڴ��е�ƫ��
	LPVOID CallOffsetAddr = (char*)BeginCode + 0x8 + 0x1;
	*(DWORD*)CallOffsetAddr = (DWORD)MessageBoxA - ((DWORD)CallOffsetAddr + 0x4 - (DWORD)vFileBuffer + pFileSign->ImageBase);

	//����jmp		OEP-(BeginCode+Push+Call+Jmp-ImageBuffer+ImageBase)			OEP���ڴ��е�ƫ��
	LPVOID JmpOffsetAddr = (DWORD)CallOffsetAddr + 0x4 + 0x1;
	*(DWORD*)JmpOffsetAddr = (pFileSign->EntryPoint + pFileSign->ImageBase) - ((DWORD)JmpOffsetAddr + 0x4 - (DWORD)vFileBuffer + pFileSign->ImageBase);

	//����OEP
	//EntryPoint ������ʼ��ƫ��
	LPVOID pOEP = (DWORD)vFileBuffer + (*(DWORD*)((DWORD)vFileBuffer + 0x3C)) + (0x18 + 0x10);
	*(DWORD*)pOEP = (DWORD)BeginCode - (DWORD)vFileBuffer;

}


//��Shellcodeд���·���Ľ���
void _WriteShellCodeToNewSection(OUT LPVOID vFileBuffer, IN struct SectionTable* pSectionTable, IN struct FileSign* pFileSign, IN LPSTR ShellCode, IN size_t SizeOfShellcode) {

	//��λ�½ڵ�λ��
	pSectionTable += pFileSign->NumberOfSection - 1;
	LPVOID pNewSection = (DWORD)vFileBuffer + pSectionTable->VirtualAddress;

	//ȫ����0
	memset(pNewSection, 0, pSectionTable->SizeOfRawData);

	//CopyShellcode
	memcpy(pNewSection, ShellCode, SizeOfShellcode);

	//��ת����ImageBuffer����
	//����call		FunctionAddress-(BeginCode+Push+5-ImageBuffer+ImageBase)	MessageBoxA���ڴ��е�ƫ��
	LPVOID CallOffsetAddr = (char*)pNewSection + 0x8 + 0x1;
	*(DWORD*)CallOffsetAddr = (DWORD)MessageBoxA - ((DWORD)CallOffsetAddr + 0x4 - (DWORD)vFileBuffer + pFileSign->ImageBase);

	//����jmp		OEP-(BeginCode+Push+Call+Jmp-ImageBuffer+ImageBase)			OEP���ڴ��е�ƫ��
	LPVOID JmpOffsetAddr = (DWORD)CallOffsetAddr + 0x4 + 0x1;
	*(DWORD*)JmpOffsetAddr = (pFileSign->EntryPoint + pFileSign->ImageBase) - ((DWORD)JmpOffsetAddr + 0x4 - (DWORD)vFileBuffer + pFileSign->ImageBase);

	//����OEP
	//EntryPoint ������ʼ��ƫ��
	LPVOID pOEP = (DWORD)vFileBuffer + (*(DWORD*)((DWORD)vFileBuffer + 0x3C)) + (0x18 + 0x10);
	*(DWORD*)pOEP = (DWORD)pNewSection - (DWORD)vFileBuffer;
}




//д���µĽڣ��贫��ImageBuffer��Headerͷ���ڱ����ݣ��½ڵ�����
//����д
LPVOID _AddNewSection(OUT LPVOID FileBuffer,IN LPCSTR SectionName,IN size_t SizeOfSection) {
	//��ȡPE��Ϣ
	PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)FileBuffer;
	PIMAGE_NT_HEADERS NTHeader = (PIMAGE_NT_HEADERS)((DWORD)FileBuffer + DosHeader->e_lfanew);
	if (NTHeader->Signature != IMAGE_NT_SIGNATURE) {
		printf("[-]_AddNewSection:������Ч��PEͷ\n");
		return NULL;
	}
	PIMAGE_SECTION_HEADER SectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)&NTHeader->OptionalHeader + NTHeader->FileHeader.SizeOfOptionalHeader);

	//�жϿռ��Ƿ��㹻
	PIMAGE_SECTION_HEADER NewSectionHeader = SectionHeader + NTHeader->FileHeader.NumberOfSections;
	BOOL Judge = FALSE;
	for (int i = 0; i < 80; i++) {
		if ((*(BYTE*)((DWORD)NewSectionHeader + i))) {
			printf("[-]_AddNewSection:�ڱ��ռ䲻��������������\n");
			Judge = TRUE;
			break;
		}
	}
	//��NTͷ��ǰŲ
	if (Judge) {
		DosHeader->e_lfanew = 0x40;
		NTHeader = (PIMAGE_NT_HEADERS)((DWORD)FileBuffer + DosHeader->e_lfanew);
		SectionHeader = (DWORD)&NTHeader->OptionalHeader + NTHeader->FileHeader.SizeOfOptionalHeader;
		NewSectionHeader = SectionHeader + NTHeader->FileHeader.NumberOfSections;
		printf("[+]_AddNewSection:NTHeader����%x\n", NTHeader);
		//�ٴ��жϿռ��Ƿ����
		for (int i = 0; i < 80; i++) {
			if ((*(BYTE*)((DWORD)NewSectionHeader + i))) {
				printf("[-]_AddNewSection:�ڱ��ռ����ɲ���������������\n");
				Judge = TRUE;
				break;
			}
		}
		if (Judge) {
			return NULL;
		}
	}

	//�޸�NumberOfSection
	NTHeader->FileHeader.NumberOfSections += 1;
	printf("[+]_AddNewSection:�ڱ�����0x%x->0x%x\n", NTHeader->FileHeader.NumberOfSections-1, NTHeader->FileHeader.NumberOfSections);

	//д���µĽڱ�
	memcpy(NewSectionHeader, SectionHeader, sizeof(IMAGE_SECTION_HEADER));
	printf("[+]_AddNewSection:�½ڱ���ַ:0x%x\n", NewSectionHeader);
	memset(NewSectionHeader, 0, 0x8);
	memcpy(NewSectionHeader, SectionName, 0x8);
	printf("[+]_AddNewSection:�½ڱ�����:%s\n", NewSectionHeader->Name);
	NewSectionHeader->Misc.VirtualSize = SizeOfSection;
	printf("[+]_AddNewSection:�½ڱ�VirtualSize:0x%x\n", NewSectionHeader->Misc.VirtualSize);
	NewSectionHeader->VirtualAddress = (NewSectionHeader-1)->VirtualAddress + (NewSectionHeader-1)->SizeOfRawData;
	printf("[+]_AddNewSection:�½ڱ�VritualAddress:0x%x\n", NewSectionHeader->VirtualAddress);
	NewSectionHeader->SizeOfRawData = SizeOfSection;
	printf("[+]_AddNewSection:�½ڱ�SizeOfRawData:0x%x\n", NewSectionHeader->SizeOfRawData);
	NewSectionHeader->PointerToRawData = (NewSectionHeader - 1)->PointerToRawData + (NewSectionHeader - 1)->SizeOfRawData;
	printf("[+]_AddNewSection:�½ڱ�PointerToRawData:0x%x\n", NewSectionHeader->PointerToRawData);
	NewSectionHeader->Characteristics |= (NewSectionHeader - 1)->Characteristics;
	printf("[+]_AddNewSection:�½ڱ�Characteristics:0x%x\n", NewSectionHeader->Characteristics);

	//��ʼ����
	memset((DWORD)FileBuffer + NewSectionHeader->PointerToRawData, 0, NewSectionHeader->SizeOfRawData);
	
	return NewSectionHeader;

}




//�ͷ�ImageBuffer
//��ImageBuffer��ԭΪFileBuffer
size_t _NewBuffer(IN LPVOID *vFileBuffer, IN struct SectionTable* pSectionTable, IN struct FileSign* pFileSign, IN size_t SizeOfCode,OUT LPVOID* NewBuffer) {

	pSectionTable += pFileSign->NumberOfSection-1;
	size_t FileSize = pSectionTable->PointToRawData + pSectionTable->SizeOfRawData;
	pSectionTable -= pFileSign->NumberOfSection - 1;
	//�����ڴ�
	*NewBuffer = malloc(FileSize);
	if (!*NewBuffer) {
		printf("�����ڴ�ʧ��\n");
		free(*NewBuffer);
		return NULL;
	}
	memset(*NewBuffer, 0, FileSize);
	//copyPEͷ
	memcpy(*NewBuffer, *vFileBuffer, pFileSign->SizeOfHeaders);

	DWORD CodeSection = _FindCodeSection(pFileSign, pSectionTable);

	//ѭ��copy�ڱ�
	for (int i = 0; i < pFileSign->NumberOfSection; i++) {
		/*if (i == CodeSection) {
			pSectionTable->VirtualSize += SizeOfCode;
		}*/
		memcpy((DWORD)*NewBuffer + pSectionTable->PointToRawData, (DWORD)*vFileBuffer + pSectionTable->VirtualAddress, pSectionTable->SizeOfRawData);
		pSectionTable++;
	}
	//������NewBuffer���ͷ�vFileBuffer
	VirtualFree(*vFileBuffer, pFileSign->SizeOfImage, MEM_COMMIT | MEM_RESERVE);
	*vFileBuffer = NULL;


	return FileSize;
}



//��NewBuffer����
void _SaveFile(IN LPVOID* NewBuffer, IN size_t FileSize, IN LPSTR New_FilePATH) {
	FILE* pf = fopen(New_FilePATH, "wb");
	if (!pf) {
		perror("�����ļ�ʧ��");
		fclose(pf);
		return;
	}
	if (!fwrite(*NewBuffer, FileSize, 1, pf)) {
		perror("д��ʧ��");
		fclose(pf);
		return;
	}
	printf("���̳ɹ�\n");
	fclose(pf);

	free(*NewBuffer);
	*NewBuffer = NULL;
	return;
}




//���PE�ṹ�ؼ��ֶ�
void _OutputPEData(IN struct FileSign* pFileSign, IN struct SectionTable* pSectionTable) {
	printf("**********************************************************\n");
	printf("PEͷ:\n\n");
	//���PEͷ
	printf("Machine:0x%x\n", pFileSign->Machine);
	printf("NumberOfSection:0x%x\n", pFileSign->NumberOfSection);
	printf("SizeOfOptionHeader:0x%x\n\n", pFileSign->SizeOfOptionHeader);

	//�����ѡPEͷ
	printf("��ѡPEͷ:\n\n");
	printf("Magic:0x%x\n", pFileSign->Magic);
	printf("EntryPoint:0x%x\n", pFileSign->EntryPoint);
	printf("ImageBase:0x%x\n", pFileSign->ImageBase);
	printf("SectionAlignment:0x%x\n", pFileSign->SectionAlignment);
	printf("FileAlignment:0x%x\n", pFileSign->FileAlignment);
	printf("SizeOfImage:0x%x\n", pFileSign->SizeOfImage);
	printf("SizeOfHeaders:0x%x\n\n", pFileSign->SizeOfHeaders);

	printf("�ڱ�:\n\n");
	for (int i = 0; i < pFileSign->NumberOfSection; i++) {
		printf("name:%s\n", pSectionTable->name);
		printf("VirtualSize:0x%x\n", pSectionTable->VirtualSize);
		printf("VirtualAddress:0x%x\n", pSectionTable->VirtualAddress);
		printf("SizeOfRawData:0x%x\n", pSectionTable->SizeOfRawData);
		printf("PointToRawData:0x%x\n", pSectionTable->PointToRawData);
		printf("Characteristics:0x%x\n\n", pSectionTable->Characteristics);
		pSectionTable++;
	}
	printf("**********************************************************\n");
	system("pause");
}


//�����ļ����룬���ض����Ĵ�С
size_t _MemoryAlign(size_t FileSize, size_t Align) {
	size_t n = FileSize / Align;
	if (FileSize % Align > 1) {
		n += 1;
	}
	return n * Align;
}



//���ںϲ�Ϊһ��
void _Mergesection(LPVOID vFileBuffer, struct FileSign* pFileSign,struct SectionTable* pSectionTable,LPSTR SectionName) {
	LPVOID pNumberOfSection = (WORD*)((DWORD)vFileBuffer + *(DWORD*)((DWORD)vFileBuffer + 0x3C) + 0x6);
	*(WORD*)pNumberOfSection = 1;

	//��λ�ڱ�
	LPVOID BeginSection = (DWORD)vFileBuffer + *(DWORD*)((DWORD)vFileBuffer + 0x3c) + 0x18 + pFileSign->SizeOfOptionHeader;

	//���Ľڵ�����
	memcpy(BeginSection, SectionName, 8);


	//��λ��ԭ���һ���ڵ�λ��
	pSectionTable += pFileSign->NumberOfSection - 1;
	

	//����VirtualSize
	LPVOID pVirtualSize = (DWORD)BeginSection + 0x8;
	*(DWORD*)pVirtualSize = pSectionTable->VirtualAddress + pSectionTable->SizeOfRawData - _MemoryAlign(pFileSign->SizeOfHeaders, pFileSign->SectionAlignment);

	//����SizeOfRawData
	LPVOID pSizeOfRawData = (DWORD)BeginSection + 0x10;
	*(DWORD*)pSizeOfRawData = pSectionTable->VirtualAddress + pSectionTable->SizeOfRawData - _MemoryAlign(pFileSign->SizeOfHeaders, pFileSign->SectionAlignment);
	

}



//Ϊ���һ��������
void _ExpansionSection(OUT LPVOID FileBuffer,IN struct FileSign* pFileSign,IN struct SectionTable* pSectionTable,IN size_t ExpansionSize) {

	//�޸�SizeOfImage
	LPVOID pSizeOfImage = (DWORD)FileBuffer + *(DWORD*)((DWORD)FileBuffer + 0x3C) + 0x18 + 0x38;
	*(DWORD*)pSizeOfImage += ExpansionSize;


	//��λ���һ����
	pSectionTable += pFileSign->NumberOfSection - 1;
	

	//�޸�VirtualSize
	LPVOID pVirtualSize = (DWORD)pSectionTable->Point + 0x8;
	*(DWORD*)pVirtualSize += ExpansionSize;

	//�޸�SizeOfRawData
	LPVOID pSizeOfRawData = (DWORD)pSectionTable->Point + 0x10;
	*(DWORD*)pSizeOfRawData += ExpansionSize;



}

DWORD _FindRVASection(LPVOID FileBuffer, DWORD RVA,struct FileSign* pFileSign,struct SectionTable* pSectionTable) {
	for (int i = 0; i < pFileSign->NumberOfSection; i++) {
		if (RVA >= pSectionTable->VirtualAddress && RVA < (pSectionTable->VirtualAddress + pSectionTable->SizeOfRawData)) {
			return i;
		}
		pSectionTable++;
	}
}


//����д
DWORD _RVAToFOA(LPVOID FileBuffer,DWORD RVA) {
	
	PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)FileBuffer;
	PIMAGE_NT_HEADERS NTHeader = (PIMAGE_NT_HEADERS)((DWORD)FileBuffer + DosHeader->e_lfanew);
	PIMAGE_SECTION_HEADER SectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)&NTHeader->OptionalHeader + NTHeader->FileHeader.SizeOfOptionalHeader);

	//��λ���ڽ�
	int i;
	for (i = 0; i < NTHeader->FileHeader.NumberOfSections - 1; i++) {
		if (RVA >= SectionHeader[i].VirtualAddress && RVA < SectionHeader[i + 1].VirtualAddress)
			break;
	}
	SectionHeader += i;
	//���ƫ��
	DWORD Offset = RVA - SectionHeader->VirtualAddress;
	return SectionHeader->PointerToRawData + Offset;

}


//����ΪFileBuffer��ҪѰ�ҵĺ��������֣����غ�����FileBuffer�еĵ�ַ
LPVOID _GetFunctionAddrByName(LPVOID FileBuffer, LPSTR FunctionName) {

	//��ȡPE�ؼ��ֶ�
	struct FileSign pFileSign;
	_ReadData(FileBuffer, &pFileSign);

	//��ȡ�ڱ��ؼ��ֶ�
	struct SectionTable* pSectionTable = malloc((sizeof(pSectionTable)) * (pFileSign.NumberOfSection));
	if (!pSectionTable) {
		printf("pSectionTable����ʧ��\n");
		return NULL;
	}
	_ReadSectionTable(pSectionTable, &pFileSign);


	//��ȡ��������FOA
	DWORD ExportTable_RVA = *(DWORD*)((DWORD)pFileSign.OptionalHeader + pFileSign.SizeOfOptionHeader - (8 * 16));
	DWORD ExportTable_FOA = _RVAToFOA(FileBuffer, ExportTable_RVA);
	LPVOID ExportTable = (DWORD)FileBuffer + ExportTable_FOA;


	//��ȡ���������ָ���
	DWORD NumberOfName = *(DWORD*)((DWORD)ExportTable + 0x18);


	//������ַ��
	LPVOID pAddrTable = (DWORD)ExportTable + 0x1C;
	DWORD pAddrTable_FOA = _RVAToFOA(FileBuffer, *(DWORD*)pAddrTable);
	DWORD* AddrTable = pAddrTable_FOA + (DWORD)FileBuffer;

	//�������Ʊ�
	LPVOID pNameTable = (DWORD)ExportTable + 0x20;
	DWORD pNameTable_FOA = _RVAToFOA(FileBuffer, *(DWORD*)pNameTable);
	DWORD* NameTable = pNameTable_FOA + (DWORD)FileBuffer;
	//������ű�
	LPVOID pNumberTable = (DWORD)ExportTable + 0x24;
	DWORD pNumberTable_FOA = _RVAToFOA(FileBuffer, *(DWORD*)pNumberTable);
	WORD* NumberTable = pNumberTable_FOA + (DWORD)FileBuffer;


	//ѭ���Ա����ƣ�����i��������
	int i = 0;
	for (i = 0; i < NumberOfName; i++) {
		LPSTR Name = (DWORD)FileBuffer + _RVAToFOA(FileBuffer, *(DWORD*)(NameTable));
		if (!(strcmp(Name, FunctionName))) {
			break;
		}
		NameTable++;
	}
	//�ں������Ƹ�����û���ҵ����Ӧ������ʱ,����NULL
	if (i > NumberOfName-1) {
		return NULL;
	}
	

	//ʹ������Ѱ�����
	NumberTable += i;
	WORD FunctionNumber = *(NumberTable);

	//���غ�����ַ
	AddrTable += FunctionNumber;
	LPVOID FunctionAddr = (DWORD)FileBuffer + _RVAToFOA(FileBuffer, *AddrTable);
	return FunctionAddr;
	

}

//��ȡ�ض�λ��
void _PrintReloc(LPVOID FileBuffer) {

	//��ȡPE�ṹ
	struct FileSign FileSign1;
	_ReadData(FileBuffer, &FileSign1);

	//��ȡ�ڱ�
	struct SectionTable* pSectionTable = malloc(sizeof(pSectionTable) * FileSign1.NumberOfSection);
	_ReadSectionTable(pSectionTable, &FileSign1);


	//��λ�ض�λ��
	DWORD Reloc_RVA = *(DWORD*)((DWORD)FileSign1.OptionalHeader + FileSign1.SizeOfOptionHeader - (8 * 11));
	LPVOID Reloc = (DWORD)FileBuffer + _RVAToFOA(FileBuffer, Reloc_RVA);
	

	//��ȡ�ض�λ������
	//DWORD VirtualAddr = (DWORD)FileBuffer + _RVAToFOA(FileBuffer, *(DWORD*)(Reloc));
	DWORD VirtualAddr_FOA = (DWORD)FileBuffer + _RVAToFOA(FileBuffer,*(DWORD*)(Reloc));
	DWORD SizeOfBlock = *(DWORD*)((DWORD)Reloc + 0x4);
	WORD* Point = (WORD*)((DWORD)Reloc + 0x8);
	DWORD Temp;
	DWORD Offset;

	//ѭ����ӡ���ض�λ�ĵ�ַ
	do {
		
		for (int i = 0; i < ((SizeOfBlock - 0x8) / 2); i++) {
			Temp = *(Point);
			if ((Temp & (0b1111000000000000)) == 0b0011000000000000) {
				Offset = (VirtualAddr_FOA + (Temp & 0b0000111111111111)) - (DWORD)FileBuffer;
				printf("Offset:0x%x\n", Offset);
			}
			Point++;
		}
		Reloc = (LPVOID)((DWORD)Reloc + SizeOfBlock);
		VirtualAddr_FOA = (DWORD)FileBuffer + _RVAToFOA(FileBuffer,*(DWORD*)(Reloc));
		SizeOfBlock = *(DWORD*)((DWORD)Point + 0x4);
		Point = (WORD*)((DWORD)Reloc + 0x8);
	} while (SizeOfBlock < 0xFFFF);

	return;
	
}

//��������Ų��ָ���ڣ����ؽڵĵ�ַ
LPVOID _MoveExport(IN LPVOID FileBuffer,IN LPCSTR SectionName) {
	//DOSͷ
	PIMAGE_DOS_HEADER DOSHeader = (PIMAGE_DOS_HEADER)FileBuffer;
	if (DOSHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		printf("[-]����PE�ļ�\n");
		return NULL;
	}
	printf("[+]_MoveExport:DOSHeader:0x%x\n", DOSHeader);

	//NTͷ
	PIMAGE_NT_HEADERS NTHeaders = (PIMAGE_NT_HEADERS)((DWORD)FileBuffer + DOSHeader->e_lfanew);
	if (NTHeaders->Signature != IMAGE_NT_SIGNATURE) {
		printf("[-]ȱ��PEͷ\n");
		return NULL;
	}
	printf("[+]_MoveExport:NTHeaders:0x%x\n", NTHeaders);

	//��ӡ�ڵ�����
	DWORD NumberOfSection = NTHeaders->FileHeader.NumberOfSections;
	printf("[+]_MoveExport:�ڱ�������0x%x\n", NumberOfSection);


	//���������ҳ�ָ����
	PIMAGE_SECTION_HEADER SectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)&NTHeaders->OptionalHeader + NTHeaders->FileHeader.SizeOfOptionalHeader);
	int i = 0;
	BOOL Judge = FALSE;
	for (i = 0; i < NTHeaders->FileHeader.NumberOfSections; i++) {
		if (!strcmp(SectionName, SectionHeader[i].Name)) {
			Judge = TRUE;
			break;
		}
	}
	if (!Judge) {
		printf("[-]_MoveExport:û��ָ����\n");
		return NULL;
	}
	SectionHeader += i;


	//����д��ĵط�
	LPVOID pBegin = (DWORD)FileBuffer + SectionHeader->PointerToRawData;


	//������λ��
	if (!(DWORD)NTHeaders->OptionalHeader.DataDirectory->VirtualAddress) {
		printf("[-]_MoveExport:���ļ�û�е�������\n");
		return NULL;
	}
	//��������
	PIMAGE_EXPORT_DIRECTORY ExportTable = (PIMAGE_EXPORT_DIRECTORY)((DWORD)FileBuffer + _RVAToFOA(FileBuffer, (DWORD)NTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
	PDWORD pExportNames = (DWORD)FileBuffer + _RVAToFOA(FileBuffer, ExportTable->AddressOfNames);
	LPCSTR Name;
	size_t Lenth;
	for (int i = 0; i < ExportTable->NumberOfNames; i++) {
		Name = (DWORD)FileBuffer + _RVAToFOA(FileBuffer, *(DWORD*)pExportNames);
		Lenth = strlen(Name);
		memcpy(pBegin, Name, Lenth);
		(PBYTE)pBegin += Lenth+1;
		pExportNames++;
		printf("[+]_MoveExport:��������(%s)��%s�ڳɹ�\n", Name, SectionName);
	}









	return (DWORD)SectionHeader;

}

