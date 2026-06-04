/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConnectionState.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: imutavdz <imutavdz@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/04 11:39:59 by imutavdz          #+#    #+#             */
/*   Updated: 2026/06/04 11:54:19 by imutavdz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECT_STATE
#define CONNECT_STATE

class ConnectionState
{
public:
    ConnectionState();
    ConnectionState();
    ConnectionState(const ConnectionState& other);
    ConnectionState& operator=(const ConnectionState& other);
    ~ConnectionState();


};

#endif
