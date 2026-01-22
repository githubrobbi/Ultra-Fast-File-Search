; Custom build step (x64):     ml64.exe /Fo"$(IntDir)\$(InputName).obj" /D_WIN64 /c /nologo /W3 /Zi /Ta "$(InputPath)"
; Custom build output:         $(IntDir)\$(InputName).obj
;
; http://kobyk.wordpress.com/2007/07/20/dynamically-linking-with-msvcrtdll-using-visual-c-2005/
; http://www.openrce.org/articles/full_view/21
; http://blogs.msdn.com/b/freik/archive/2006/01/04/509372.aspx

ifndef _WIN64
	.386
	.model flat, c
endif
option dotname

extern __CxxFrameHandler: PROC

ifdef _WIN64
	extern __imp___CxxFrameHandler: PROC
	extern __imp_VirtualProtect: PROC
	extern __imp_Sleep: PROC
	extern __imp_GetVersion: PROC
endif

.data

ifdef _WIN64
	;ProtectFlag EQU ?ProtectFlag@?1??__CxxFrameHandler3@@9@9
	ProtectFlag dd ?
endif

.code

ifdef _WIN64
	includelib         kernel32.lib
endif
;includelib         msvcrt.lib

public __CxxFrameHandler3

ifdef _WIN64
__CxxFrameHandler3 proc frame
else
__CxxFrameHandler3 proc
endif
	ifndef _WIN64
		push        ebp
		mov         ebp,esp
		sub         esp,28h
		push        ebx
		push        esi
		push        edi
		cld
		mov         dword ptr [ebp-4],eax
		mov         esi,dword ptr [ebp-4]
		push        9
		pop         ecx
		lea         edi,[ebp-28h]
		rep movs    dword ptr es:[edi],dword ptr [esi]
		mov         eax,dword ptr [ebp-28h]
		and         eax,0F9930520h
		or          eax,019930520h
		mov         dword ptr [ebp-28h],eax
		lea         eax,[ebp-28h]
		mov         dword ptr [ebp-4],eax
		push        dword ptr [ebp+14h]
		push        dword ptr [ebp+10h]
		push        dword ptr [ebp+0Ch]
		push        dword ptr [ebp+8]
		mov         eax,dword ptr [ebp-4]
		call        __CxxFrameHandler
		add         esp,10h
		pop         edi
		pop         esi
		pop         ebx
		mov         esp,ebp
		pop         ebp
		ret
	else
		mov	rax,rsp
		mov	qword ptr [rax+8],rbx
		.savereg	rbx, 50h
		mov	qword ptr [rax+10h],rbp
		.savereg	rbp, 58h
		mov	qword ptr [rax+18h],rsi
		.savereg	rsi, 60h
		push	rdi
		.pushreg	rdi
		push	r12
		.pushreg	r12
		push	r13
		.pushreg	r13
		sub	rsp,30h
		.allocstack	30h
		.endprolog
		mov	dword ptr [rax+20h],40h
		mov	rax,qword ptr [r9+38h]
		mov	rdi,r9
		mov	ebx,dword ptr [rax]
		mov	rsi,r8
		mov	rbp,rdx
		add	rbx,qword ptr [r9+8]
		mov	r12,rcx
		mov	eax,dword ptr [rbx]
		and	eax,1FFFFFFFh
		cmp	eax,19930520h
		je	L140001261
		mov	r13d,1
		mov	eax,r13d
		lock	xadd dword ptr [ProtectFlag],eax
		add	eax,r13d
		cmp	eax,r13d
		je	L140001217
	L1400011F0:
		lock	add dword ptr [ProtectFlag],0FFFFFFFFh
		mov	ecx,0Ah
		call	qword ptr [__imp_Sleep]
		mov	r11d,r13d
		lock	xadd dword ptr [ProtectFlag],r11d
		add	r11d,r13d
		cmp	r11d,r13d
		jne	L1400011F0
	L140001217:
		mov	r8d,dword ptr [rsp+68h]
		mov	r13d,4
		lea	r9,[rsp+20h]
		mov	rdx,r13
		mov	rcx,rbx
		call	qword ptr [__imp_VirtualProtect]
		test	eax,eax
		je	L140001259
		and	dword ptr [rbx],0F9930520h
		or	dword ptr [rbx],19930520h
		mov	r8d,dword ptr [rsp+20h]
		lea	r9,[rsp+68h]
		mov	rdx,r13
		mov	rcx,rbx
		call	qword ptr [__imp_VirtualProtect]
	L140001259:
		lock	add dword ptr [ProtectFlag],0FFFFFFFFh
	L140001261:
		mov	r9,rdi
		mov	r8,rsi
		mov	rdx,rbp
		mov	rcx,r12
		call	qword ptr [__imp___CxxFrameHandler]
		mov	rbx,qword ptr [rsp+50h]
		mov	rbp,qword ptr [rsp+58h]
		mov	rsi,qword ptr [rsp+60h]
		add	rsp,30h
		pop	r13
		pop	r12
		pop	rdi
		ret
	endif
__CxxFrameHandler3 endp

end

